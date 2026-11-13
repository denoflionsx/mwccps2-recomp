"""Discover an image's code and split it into functions.

Code is found by following control flow from the entry point, the exports, and every relocated
word that points into an executable section. A call's continuation is decoded only once its
callee is known to return, so padding after calls that never return is not mistaken for code.
"""
from collections import defaultdict

from iced_x86 import Decoder, FlowControl, Mnemonic, OpKind, Register

from pe import PEError

NO_RETURN_IMPORTS = {"KERNEL32.dll!ExitProcess"}
BRANCH = (OpKind.NEAR_BRANCH16, OpKind.NEAR_BRANCH32)
IMMEDIATE = (OpKind.IMMEDIATE32, OpKind.IMMEDIATE8TO32)


class Program:
    def __init__(self, image, seeds=()):
        self.image = image
        self.instructions = {}  # address -> decoded instruction
        self.tables = {}        # indirect jump address -> jump table targets
        self.bounded = set()    # indirect jumps whose table length a compare gives
        self.starts = {}        # every code byte -> address of its instruction
        self.relocated = set(image.relocations)
        self.discover({image.entry, *seeds, *filter(image.executable, image.exports.values())})
        calls = {instruction.near_branch_target for instruction in self.instructions.values()
                 if instruction.flow_control == FlowControl.CALL}
        self.owner = self.partition(self.seeds | calls)
        # Code reached only through words that turned out to be table entries is unreferenced.
        self.instructions = {address: self.instructions[address] for address in self.owner}
        self.functions = defaultdict(list)  # entry -> its instruction addresses, ascending
        for address in sorted(self.owner):
            self.functions[self.owner[address]].append(address)

    def import_name(self, instruction):
        """The import called or jumped to through an import address table slot, if any."""
        if (instruction.op0_kind == OpKind.MEMORY and instruction.memory_base == Register.NONE and
                instruction.memory_index == Register.NONE and instruction.segment_prefix == Register.NONE):
            return self.image.imports.get(instruction.memory_displacement)
        return None

    def decode(self, address, origin):
        image = self.image
        if not image.executable(address):
            raise PEError(f"control flow leaves the code at {address:#x} (from {origin:#x})")
        data = image.read(address, min(15, image.base + image.size - address))
        instruction = Decoder(32, data, ip=address).decode()
        if (instruction.is_invalid or not image.executable(address, instruction.len) or
                any(address + offset in self.starts for offset in range(instruction.len))):
            raise PEError(f"invalid or overlapping instruction at {address:#x} (from {origin:#x})")
        self.instructions[address] = instruction
        for offset in range(instruction.len):
            self.starts[address + offset] = address
        return instruction

    def jump_table(self, instruction, previous):
        """Targets of `jmp [index*4 + table]`, bounded by the preceding `cmp index, n; ja/jae`
        or else by the run of relocated code pointers at the table."""
        if (instruction.op0_kind != OpKind.MEMORY or instruction.memory_base != Register.NONE or
                instruction.memory_index == Register.NONE or instruction.memory_index_scale != 4 or
                instruction.segment_prefix != Register.NONE):
            return None
        table = instruction.memory_displacement
        limit = None
        if len(previous) == 2 and previous[1].mnemonic in (Mnemonic.JA, Mnemonic.JAE):
            compare, branch = previous
            if (compare.mnemonic == Mnemonic.CMP and compare.op0_kind == OpKind.REGISTER and
                    compare.op0_register == instruction.memory_index and compare.op1_kind in IMMEDIATE):
                limit = compare.immediate(1) + (branch.mnemonic == Mnemonic.JA)
        targets = []
        while len(targets) < (limit or 4096) and table + len(targets) * 4 in self.relocated:
            target = self.image.word(table + len(targets) * 4)
            if not self.image.executable(target):
                break
            targets.append(target)
        if limit:
            self.bounded.add(instruction.ip)
        return targets or None

    def discover(self, seeds):
        image = self.image
        self.seeds = set(seeds)
        owner = {}                     # instruction -> the entry it was first reached from
        returns = set()                # entries known to reach a return
        waiting = defaultdict(list)    # callee -> continuations to decode once it returns
        flows = defaultdict(set)       # entry -> entries that return if it does
        todo = [(seed, seed) for seed in sorted(self.seeds)]

        def returned(entry):
            pending = [entry]
            while pending:
                entry = pending.pop()
                if entry not in returns:
                    returns.add(entry)
                    todo.extend(waiting.pop(entry, ()))
                    pending.extend(flows.pop(entry, ()))

        def walk(address, entry):
            previous = []
            while True:
                if address in self.instructions:
                    if owner[address] in returns:
                        returned(entry)
                    elif owner[address] != entry:
                        flows[owner[address]].add(entry)
                    return
                instruction = self.decode(address, entry)
                owner[address] = entry
                flow = instruction.flow_control
                target = instruction.near_branch_target if instruction.op0_kind in BRANCH else None
                if flow == FlowControl.CALL:
                    todo.append((target, target))
                    if target in returns:
                        todo.append((instruction.next_ip, entry))
                    else:
                        waiting[target].append((instruction.next_ip, entry))
                    return
                if flow == FlowControl.INDIRECT_CALL and self.import_name(instruction) in NO_RETURN_IMPORTS:
                    return
                if flow == FlowControl.UNCONDITIONAL_BRANCH:
                    address, previous = target, []
                    continue
                if flow == FlowControl.INDIRECT_BRANCH:
                    targets = self.jump_table(instruction, previous)
                    if targets:
                        self.tables[address] = targets
                        todo.extend((target, entry) for target in targets)
                    elif self.import_name(instruction) not in NO_RETURN_IMPORTS:
                        returned(entry)
                    return
                if flow == FlowControl.RETURN:
                    returned(entry)
                    return
                if flow == FlowControl.CONDITIONAL_BRANCH:
                    todo.append((target, entry))
                elif flow not in (FlowControl.NEXT, FlowControl.INDIRECT_CALL):
                    return
                address, previous = instruction.next_ip, [*previous[-1:], instruction]

        def code_pointers():
            """Relocated words that hold code addresses: those outside instructions and bounded
            jump tables, and those that are an instruction's immediate operand. A table without
            a bound may run on into function pointers, so its words count as both."""
            table_words = {self.instructions[address].memory_displacement + index * 4
                           for address in self.bounded for index in range(len(self.tables[address]))}
            pointers = set()
            for slot in self.relocated - table_words:
                target = image.word(slot)
                instruction = self.instructions.get(self.starts.get(slot))
                if image.executable(target) and (not image.executable(slot) or (instruction and any(
                        instruction.op_kind(index) in IMMEDIATE and instruction.immediate(index) == target
                        for index in range(instruction.op_count)))):
                    pointers.add(target)
            return pointers

        while todo:
            while todo:
                walk(*todo.pop())
            found = code_pointers() - self.seeds
            self.seeds |= found
            todo.extend((target, target) for target in sorted(found))
        # A table found late had its entries taken for code pointers; they are not functions.
        self.seeds = set(seeds) | code_pointers()

    def successors(self, instruction):
        """Addresses control can reach from an instruction without leaving its function's frame."""
        flow = instruction.flow_control
        if flow == FlowControl.UNCONDITIONAL_BRANCH:
            return [instruction.near_branch_target]
        if flow == FlowControl.INDIRECT_BRANCH:
            return self.tables.get(instruction.ip, [])
        if flow == FlowControl.CONDITIONAL_BRANCH:
            return [instruction.near_branch_target, instruction.next_ip]
        if flow in (FlowControl.NEXT, FlowControl.CALL, FlowControl.INDIRECT_CALL):
            return [instruction.next_ip] if instruction.next_ip in self.instructions else []
        return []

    def partition(self, entries):
        """Give each instruction one owning entry. Code reached from two entries becomes an entry
        itself, so every jump between functions is a tail call."""
        while True:
            owner = {}
            shared = set()
            for entry in sorted(entries):
                todo = [entry]
                while todo:
                    address = todo.pop()
                    if address in owner or (address in entries and address != entry):
                        if address not in entries and owner[address] != entry:
                            shared.add(address)
                        continue
                    owner[address] = entry
                    todo.extend(self.successors(self.instructions[address]))
            if not shared:
                self.entries = entries
                return owner
            entries = entries | shared

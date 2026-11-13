"""Translate each discovered function to one C++ function over the contract in runtime/cpu.h.

Status flags are computed only where a later instruction reads them. A condition whose flags
come from the nearest preceding compare, test, or arithmetic result in straight-line code is
emitted as a direct comparison of that instruction's operands.
"""
from collections import defaultdict

from iced_x86 import (Code, FlowControl, Formatter, FormatterSyntax, MemorySizeExt, Mnemonic as M, OpKind,
                      Register, RegisterExt)

OF, SF, ZF, AF, CF, PF = 1, 2, 4, 8, 16, 32  # iced_x86.RflagsBits
STATUS = OF | SF | ZF | AF | CF | PF

REGISTERS = {getattr(Register, name.upper()): f"cpu.{name}" for name in (
    "eax ecx edx ebx ebp esi edi ax cx dx bx bp si di al cl dl bl ah ch dh bh").split()}
REGISTERS[Register.ESP] = "esp"  # a local: passed to and returned by every function
IMMEDIATES = (OpKind.IMMEDIATE8, OpKind.IMMEDIATE16, OpKind.IMMEDIATE32, OpKind.IMMEDIATE8TO16,
              OpKind.IMMEDIATE8TO32)
ARITHMETIC = {M.ADD: "+", M.ADC: "+", M.SUB: "-", M.SBB: "-", M.CMP: "-"}
LOGIC = {M.AND: "&", M.OR: "|", M.XOR: "^", M.TEST: "&"}
SHIFTS = {M.SHL: "Left", M.SHR: "Right", M.SAR: "Arithmetic", M.ROL: "RotateLeft", M.ROR: "RotateRight",
          M.RCL: "CarryLeft", M.RCR: "CarryRight"}
STRINGS = {M.MOVSB: "Move", M.MOVSW: "Move", M.MOVSD: "Move", M.STOSB: "Store", M.STOSW: "Store",
           M.STOSD: "Store", M.LODSB: "Load", M.LODSW: "Load", M.LODSD: "Load", M.CMPSB: "Compare",
           M.CMPSW: "Compare", M.CMPSD: "Compare", M.SCASB: "Scan", M.SCASW: "Scan", M.SCASD: "Scan"}
# x87 forms: code -> runtime call, formatted with the memory address or stack index.
X87 = {
    Code.FLD_M32FP: "X87_Load({m}, 32, false)", Code.FLD_M64FP: "X87_Load({m}, 64, false)",
    Code.FLD_M80FP: "X87_Load({m}, 80, false)", Code.FILD_M32INT: "X87_Load({m}, 32, true)",
    Code.FILD_M64INT: "X87_Load({m}, 64, true)",
    Code.FSTP_M32FP: "X87_Store({m}, 32, false, true)", Code.FSTP_M64FP: "X87_Store({m}, 64, false, true)",
    Code.FST_M64FP: "X87_Store({m}, 64, false, false)", Code.FISTP_M32INT: "X87_Store({m}, 32, true, true)",
    Code.FCOM_M64FP: "X87_Compare({m}, true, 0)", Code.FCOMP_M64FP: "X87_Compare({m}, true, 1)",
    Code.FCOM_ST0_STI: "X87_Compare({i}, false, 0)", Code.FCOMP_ST0_STI: "X87_Compare({i}, false, 1)",
    Code.FCOMPP: "X87_Compare(1, false, 2)",
    Code.FBLD_M80BCD: "X87_Decimal({m}, false)", Code.FBSTP_M80BCD: "X87_Decimal({m}, true)",
    Code.FADD_M32FP: "X87_Arithmetic(FloatOperation::Add, {m}, 32, 0, false, false)",
    Code.FADD_M64FP: "X87_Arithmetic(FloatOperation::Add, {m}, 64, 0, false, false)",
    Code.FSUB_M32FP: "X87_Arithmetic(FloatOperation::Subtract, {m}, 32, 0, false, false)",
    Code.FSUB_M64FP: "X87_Arithmetic(FloatOperation::Subtract, {m}, 64, 0, false, false)",
    Code.FMUL_M64FP: "X87_Arithmetic(FloatOperation::Multiply, {m}, 64, 0, false, false)",
    Code.FDIV_M64FP: "X87_Arithmetic(FloatOperation::Divide, {m}, 64, 0, false, false)",
    Code.FADD_ST0_STI: "X87_Arithmetic(FloatOperation::Add, {i}, 0, 0, false, false)",
    Code.FSUBP_STI_ST0: "X87_Arithmetic(FloatOperation::Subtract, 0, 0, {i}, false, true)",
    Code.FMULP_STI_ST0: "X87_Arithmetic(FloatOperation::Multiply, 0, 0, {i}, false, true)",
    Code.FDIVP_STI_ST0: "X87_Arithmetic(FloatOperation::Divide, 0, 0, {i}, false, true)",
    Code.FDIVRP_STI_ST0: "X87_Arithmetic(FloatOperation::Divide, 0, 0, {i}, true, true)",
    Code.FNINIT: "X87_Initialize()", Code.FNSTCW_M2BYTE: "W16({m}, cpu.x87.Control)",
    Code.FNSTSW_AX: "cpu.ax = cpu.x87.Status", Code.FLDCW_M2BYTE: "X87_Set_Control(R16({m}))",
    Code.WAIT: "X87_Wait()", Code.FLDZ: "X87_Load_Integer(0)", Code.FLD1: "X87_Load_Integer(1)",
    Code.FLD_STI: "X87_Load_Register({i})", Code.FSTP_STI: "X87_Store_Register({i})",
    Code.FABS: "X87_Change_Sign(true)", Code.FCHS: "X87_Change_Sign(false)",
    Code.FXCH_ST0_STI: "X87_Exchange({i})", Code.FLDLG2: "X87_Load_Log10_2()", Code.FXTRACT: "X87_Extract()",
}


class Unsupported(ValueError):
    pass


def address_of(instruction, wrapped=False):
    """The C++ expression for a memory operand's guest address. Unless `wrapped`, a small
    displacement is added in 64 bits, which the host folds into its addressing mode: the sum
    only leaves the 32-bit range next to guest pages that are never mapped."""
    terms = []
    if instruction.segment_prefix == Register.FS:
        terms.append("cpu.fs")
    elif instruction.segment_prefix != Register.NONE or instruction.memory_displ_size == 2:
        raise Unsupported("addressing mode")
    if instruction.memory_base != Register.NONE:
        terms.append(REGISTERS[instruction.memory_base])
    if instruction.memory_index != Register.NONE:
        terms.append(f"{REGISTERS[instruction.memory_index]} * {instruction.memory_index_scale}u")
    displacement = instruction.memory_displacement & 0xffffffff
    signed = displacement - (displacement >> 31 << 32)
    if terms and not wrapped and 0 < abs(signed) < 0x10000:
        return f"s64{{{' + '.join(terms)}}} {'-' if signed < 0 else '+'} 0x{abs(signed):x}"
    if displacement or not terms:
        terms.append(f"0x{displacement:x}u")
    return " + ".join(terms)


def width_of(instruction, index=0):
    kind = instruction.op_kind(index)
    if kind == OpKind.REGISTER and instruction.op_register(index) in REGISTERS:
        return RegisterExt.size(instruction.op_register(index)) * 8
    if kind == OpKind.MEMORY and MemorySizeExt.size(instruction.memory_size) in (1, 2, 4):
        return MemorySizeExt.size(instruction.memory_size) * 8
    raise Unsupported("operand")


def read(instruction, index, width=None):
    kind = instruction.op_kind(index)
    if kind in IMMEDIATES:
        return f"0x{instruction.immediate(index) & ((1 << (width or 32)) - 1):x}u"
    if kind == OpKind.REGISTER:
        width_of(instruction, index)
        return REGISTERS[instruction.op_register(index)]
    return f"R{width_of(instruction, index)}({address_of(instruction)})"


def write(instruction, index, value):
    if instruction.op_kind(index) == OpKind.REGISTER:
        width_of(instruction, index)
        return f"{REGISTERS[instruction.op_register(index)]} = {value};"
    return f"W{width_of(instruction, index)}({address_of(instruction)}, {value});"


def shift_count(instruction):
    """A shift's constant count, or None when it comes from CL."""
    return instruction.immediate(1) & 31 if instruction.op_kind(1) in IMMEDIATES else None


def is_repeated(instruction):
    return instruction.has_rep_prefix or instruction.has_repne_prefix


def flag_effects(instruction):
    """(flags read, flags always written). An instruction that may leave flags unchanged, a
    shift by CL or a repeated string instruction, writes none: earlier values pass through."""
    reads = instruction.rflags_read & STATUS
    writes = (instruction.rflags_written | instruction.rflags_cleared | instruction.rflags_set) & STATUS
    mnemonic = instruction.mnemonic
    if mnemonic in SHIFTS and shift_count(instruction) == 0:
        return 0, 0
    conditional = (mnemonic in SHIFTS and shift_count(instruction) is None) or mnemonic in (M.SHLD, M.SHRD) or (
        mnemonic in STRINGS and is_repeated(instruction))
    return reads, 0 if conditional else writes


def fused_condition(producer, code, a, b, r):
    """A condition (x86 condition number) on a producer's saved operands, or None to read flags."""
    width = width_of(producer)
    signed = f"(s{width})"
    mnemonic = producer.mnemonic
    if mnemonic in (M.CMP, M.SUB):
        difference = f"(u{width})({a} - {b})"
        return [f"SubOverflow<u{width}>({a}, {b}, {difference})", f"{a} < {b}", f"{a} == {b}", f"{a} <= {b}",
                f"{signed}{difference} < 0", f"Parity({difference})", f"{signed}{a} < {signed}{b}",
                f"{signed}{a} <= {signed}{b}"][code]
    if mnemonic in LOGIC:
        return ["false", "false", f"{r} == 0", f"{r} == 0", f"{signed}{r} < 0", f"Parity({r})",
                f"{signed}{r} < 0", f"{signed}{r} <= 0"][code]
    if mnemonic in (M.ADD, M.INC, M.DEC) and code in (2, 4):
        return f"{r} == 0" if code == 2 else f"{signed}{r} < 0"
    return None


class Emitter:
    def __init__(self, program, hooks=(), imports="Imports"):
        self.program = program
        self.hooks = set(hooks)
        self.imports = imports
        self.slots = {slot: index for index, slot in enumerate(sorted(program.image.imports))}
        self.formatter = Formatter(FormatterSyntax.INTEL)
        self.unsupported = defaultdict(int)
        instructions = program.instructions
        self.labels = set(program.entries) | self.hooks
        for instruction in instructions.values():
            if instruction.flow_control in (FlowControl.CONDITIONAL_BRANCH, FlowControl.UNCONDITIONAL_BRANCH):
                self.labels.add(instruction.near_branch_target)
            self.labels.update(program.tables.get(instruction.ip, ()))
        self.fuse()
        self.find_live_flags()

    def fuse(self):
        """Pair each conditional with the instruction that produced its flags, where the two are
        joined by straight-line code and the condition can be recomputed from saved operands."""
        instructions = self.program.instructions
        previous = {instruction.next_ip: instruction for instruction in instructions.values()
                    if instruction.flow_control in (FlowControl.NEXT, FlowControl.CONDITIONAL_BRANCH)}
        self.fused = {}                    # conditional address -> producer address
        self.saved = defaultdict(set)      # producer address -> saved operand names
        for address, instruction in instructions.items():
            if not instruction.condition_code:
                continue
            needed = instruction.rflags_read & STATUS
            producer = previous.get(address) if address not in self.labels else None
            while producer and not (producer.rflags_modified & needed):
                producer = previous.get(producer.ip) if producer.ip not in self.labels else None
            if not producer or flag_effects(producer)[1] & needed != needed:
                continue
            try:
                condition = fused_condition(producer, (instruction.condition_code - 1) >> 1, "$a", "$b", "$r")
            except Unsupported:
                continue
            if condition:
                self.fused[address] = producer.ip
                self.saved[producer.ip].update(name for name in "abr" if "$" + name in condition)

    def find_live_flags(self):
        """Backward dataflow: the flags each instruction must leave in the CPU state. Flags do
        not survive calls or returns."""
        program = self.program
        effects = {address: (0, 0) if address in self.fused else flag_effects(instruction)
                   for address, instruction in program.instructions.items()}
        successors = {}
        predecessors = defaultdict(list)
        for address, instruction in program.instructions.items():
            calls = instruction.flow_control in (FlowControl.CALL, FlowControl.INDIRECT_CALL)
            successors[address] = [] if calls else program.successors(instruction)
            for successor in successors[address]:
                predecessors[successor].append(address)
        live_in = defaultdict(int)
        self.live = defaultdict(int)       # address -> flags live after the instruction
        todo = list(program.instructions)
        while todo:
            address = todo.pop()
            live = 0
            for successor in successors[address]:
                live |= live_in[successor]
            reads, writes = effects[address]
            self.live[address] = live
            if live_in[address] != reads | (live & ~writes):
                live_in[address] = reads | (live & ~writes)
                todo.extend(predecessors[address])

    def condition(self, instruction):
        code = instruction.condition_code - 1
        producer = self.fused.get(instruction.ip)
        if producer:
            names = [f"{name}{producer:x}" for name in "abr"]
            text = fused_condition(self.program.instructions[producer], code >> 1, *names)
        else:
            text = ["cpu.of", "cpu.cf", "cpu.zf", "(cpu.cf | cpu.zf)", "cpu.sf", "cpu.pf",
                    "(cpu.sf != cpu.of)", "(cpu.zf | (cpu.sf != cpu.of))"][code >> 1]
        return f"!({text})" if code & 1 else text

    def result_flags(self, address, width, carry=None, overflow=None, auxiliary="Auxiliary(a, b, r)"):
        """Assignments for the live flags of an instruction with operands a, b and result r."""
        values = {CF: carry, OF: overflow, AF: auxiliary, ZF: "r == 0", SF: f"Sign<u{width}>(r)",
                  PF: "Parity(r)"}
        names = {CF: "cf", OF: "of", AF: "af", ZF: "zf", SF: "sf", PF: "pf"}
        live = self.live[address] & flag_effects(self.program.instructions[address])[1]
        return [f"cpu.{names[flag]} = {values[flag]};" for flag in values if live & flag and values[flag]]

    def save(self, address):
        return [f"{name}{address:x} = {name};" for name in sorted(self.saved.get(address, ()))]

    def transfer(self, target, entry):
        """Continue at a guest address: a label in this function, or another function's entry."""
        if self.program.owner[target] == entry:
            self.used_labels.add(target)
            return f"goto L{target:x};"
        return f"TAIL(sub_{target:x});"

    def modify(self, instruction, body, width, writes=True):
        """Lines for a read-modify-write of operand 0; `body` computes r from a (and b)."""
        lines = []
        if instruction.op0_kind == OpKind.MEMORY:
            lines.append(f"s64 ea = {address_of(instruction)};")
            source, store = f"R{width}(ea)", f"W{width}(ea, r);"
        else:
            source = REGISTERS[instruction.op0_register]
            store = f"{source} = r;"
        lines.append(f"u{width} a = {source}{body}")
        if writes:
            lines.append(store)
        return lines

    def instruction(self, instruction, entry):
        """C++ statements for one instruction, excluding fall-through to the next."""
        address = instruction.ip
        mnemonic = instruction.mnemonic
        flow = instruction.flow_control
        next_ip = f"0x{instruction.next_ip:x}u"
        repeated = is_repeated(instruction)
        if instruction.has_lock_prefix or (repeated and mnemonic not in STRINGS):
            raise Unsupported("prefix")
        if instruction.code in X87:
            memory = instruction.op0_kind == OpKind.MEMORY
            index = max([instruction.op_register(i) - Register.ST0 for i in range(instruction.op_count)
                         if instruction.op_kind(i) == OpKind.REGISTER and
                         Register.ST0 <= instruction.op_register(i) <= Register.ST7], default=0)
            return [X87[instruction.code].format(m=address_of(instruction, True) if memory else "", i=index) + ";"]
        if mnemonic in STRINGS:
            if instruction.segment_prefix != Register.NONE:
                raise Unsupported("segment")
            repeat = 2 if instruction.has_repne_prefix else 1 if instruction.has_rep_prefix else 0
            width = MemorySizeExt.size(instruction.memory_size) * 8
            return [f"String<u{width}>(m, StringOperation::{STRINGS[mnemonic]}, {repeat});"]
        if mnemonic in (M.MOV, M.MOVZX, M.MOVSX):
            width = width_of(instruction)
            value = read(instruction, 1, width)
            if mnemonic == M.MOVSX:
                value = f"(u{width})(s{width})(s{width_of(instruction, 1)}){value}"
            return [write(instruction, 0, value)]
        if mnemonic == M.LEA:
            if instruction.segment_prefix != Register.NONE:
                raise Unsupported("segment")
            return [write(instruction, 0, address_of(instruction, True))]
        if mnemonic in ARITHMETIC or mnemonic in LOGIC:
            width = width_of(instruction)
            operator = ARITHMETIC.get(mnemonic) or LOGIC[mnemonic]
            carry = " + cpu.cf" if mnemonic == M.ADC else " - cpu.cf" if mnemonic == M.SBB else ""
            body = f", b = {read(instruction, 1, width)}, r = a {operator} b{carry};"
            if mnemonic in LOGIC:
                flags = self.result_flags(address, width, "false", "false", None)
            elif operator == "+":
                flags = self.result_flags(address, width, f"(u64{{a}} + b{carry}) >> {width}",
                                          f"AddOverflow<u{width}>(a, b, r)")
            else:
                flags = self.result_flags(address, width, f"u64{{a}} < u64{{b}}{carry.replace('-', '+')}",
                                          f"SubOverflow<u{width}>(a, b, r)")
            lines = self.modify(instruction, body, width, writes=mnemonic not in (M.CMP, M.TEST))
            return lines + flags + self.save(address)
        if mnemonic in (M.INC, M.DEC):
            width = width_of(instruction)
            sign = f"0x{1 << (width - 1):x}u"
            body = f", b = 1, r = a {'+' if mnemonic == M.INC else '-'} b;"
            overflow = f"r == {sign}" if mnemonic == M.INC else f"a == {sign}"
            return (self.modify(instruction, body, width) + self.result_flags(address, width, None, overflow) +
                    self.save(address))
        if mnemonic == M.NEG:
            width = width_of(instruction)
            return self.modify(instruction, ", b = a, r = 0 - b; a = 0;", width) + self.result_flags(
                address, width, "b != 0", f"SubOverflow<u{width}>(a, b, r)")
        if mnemonic == M.NOT:
            return self.modify(instruction, ", r = ~a;", width_of(instruction))
        if mnemonic in SHIFTS:
            width = width_of(instruction)
            count = shift_count(instruction)
            if count == 0:
                return []
            amount = "(cpu.cl & 31)" if count is None else str(count)
            live = self.live[address] & instruction.rflags_written
            value = {M.SHL: "u32{a} << ", M.SHR: "a >> ", M.SAR: f"s32{{(s{width})a}} >> "}.get(mnemonic)
            if value and not live:
                return self.modify(instruction, f", r = {value}{amount};", width)
            if value and count is not None and count < width:
                carry = f"{value.replace('<<', '>>')}{width - count if mnemonic == M.SHL else count - 1} & 1"
                overflow = {M.SHL: f"Sign<u{width}>(a ^ r)", M.SHR: f"Sign<u{width}>(a)", M.SAR: "false"}[mnemonic]
                return self.modify(instruction, f", r = {value}{count};", width) + self.result_flags(
                    address, width, carry, overflow if count == 1 else None, None)
            if mnemonic in (M.RCL, M.RCR) and count == 1 and width == 32:
                left = mnemonic == M.RCL
                body = ", r = a << 1 | cpu.cf;" if left else ", r = a >> 1 | u32{cpu.cf} << 31;"
                lines = self.modify(instruction, body, 32)
                lines.append(f"cpu.cf = {'Sign<u32>(a)' if left else 'a & 1'};")
                if live & OF:
                    lines.append("cpu.of = Sign<u32>(r) != cpu.cf;" if left else "cpu.of = Sign<u32>(r ^ r << 1);")
                return lines
            if mnemonic not in (M.SHL, M.SHR, M.SAR) and width != 32:
                raise Unsupported("narrow rotate")
            return self.modify(
                instruction, f", r = Shift<u{width}>(ShiftOperation::{SHIFTS[mnemonic]}, a, {amount});", width)
        if mnemonic in (M.SHLD, M.SHRD):
            if width_of(instruction) != 32:
                raise Unsupported("narrow double shift")
            left = str(mnemonic == M.SHLD).lower()
            body = f", r = Shift_Double(a, {read(instruction, 1)}, {read(instruction, 2)}, {left});"
            return self.modify(instruction, body, 32)
        if mnemonic in (M.MUL, M.IMUL):
            if width_of(instruction) != 32:
                raise Unsupported("narrow multiply")
            wide = "s64{(s32)" if mnemonic == M.IMUL else "u64{"
            overflow = "p != s32(p)" if mnemonic == M.IMUL else "p >> 32"
            flags = [f"cpu.cf = cpu.of = {overflow};"] if self.live[address] & (CF | OF) else []
            if instruction.op_count == 1:
                lines = [f"auto p = {wide}cpu.eax}} * {wide}{read(instruction, 0)}}};", "cpu.eax = (u32)p;",
                         "cpu.edx = (u32)(p >> 32);"]
            else:
                first = 1 if instruction.op_count == 3 else 0
                last = instruction.op_count - 1
                lines = [f"auto p = {wide}{read(instruction, first)}}} * {wide}{read(instruction, last)}}};",
                         write(instruction, 0, "(u32)p")]
            return lines + flags
        if mnemonic in (M.DIV, M.IDIV):
            if width_of(instruction) != 32:
                raise Unsupported("narrow divide")
            return [f"Divide({read(instruction, 0)}, {str(mnemonic == M.IDIV).lower()});"]
        if mnemonic == M.BSR:
            if width_of(instruction) != 32:
                raise Unsupported("narrow bit scan")
            return [f"u32 a = {read(instruction, 1)};", "cpu.zf = a == 0;",
                    "if (a) " + write(instruction, 0, "31 - __builtin_clz(a)")]
        if mnemonic == M.XCHG:
            width = width_of(instruction)
            return [f"u{width} a = {read(instruction, 0)}, b = {read(instruction, 1)};",
                    write(instruction, 0, "b"), write(instruction, 1, "a")]
        if mnemonic == M.CDQ:
            return ["cpu.edx = (u32)((s32)cpu.eax >> 31);"]
        if mnemonic == M.SAHF:
            return ["cpu.cf = cpu.ah & 1; cpu.pf = cpu.ah & 4; cpu.af = cpu.ah & 16; cpu.zf = cpu.ah & 64; "
                    "cpu.sf = cpu.ah & 128;"]
        if mnemonic in (M.CLD, M.STD):
            return [f"cpu.df = {str(mnemonic == M.STD).lower()};"]
        if mnemonic == M.NOP:
            return []
        if mnemonic == M.LEAVE and instruction.code == Code.LEAVED:
            return ["esp = cpu.ebp;", "cpu.ebp = POP();"]
        if mnemonic == M.PUSH:
            if instruction.op0_kind in (OpKind.IMMEDIATE16, OpKind.IMMEDIATE8TO16) or (
                    instruction.op0_kind not in IMMEDIATES and width_of(instruction) != 32):
                raise Unsupported("16-bit push")
            return [f"PUSH({read(instruction, 0)});"]
        if mnemonic == M.POP:
            if width_of(instruction) != 32:
                raise Unsupported("16-bit pop")
            return ["u32 a = POP();", write(instruction, 0, "a")]
        if instruction.condition_code and flow == FlowControl.NEXT:
            if instruction.op_count != 1 or width_of(instruction) != 8:
                raise Unsupported("conditional")
            return [write(instruction, 0, self.condition(instruction))]
        if flow == FlowControl.CALL:
            return [f"CALL(sub_{instruction.near_branch_target:x}, {next_ip});"]
        if flow == FlowControl.INDIRECT_CALL:
            slot = self.slots.get(instruction.memory_displacement) if self.program.import_name(instruction) else None
            if slot is not None:
                return [f"CALL({self.imports}[{slot}], {next_ip});"]
            return [f"CALL_INDIRECT({read(instruction, 0)}, {next_ip});"]
        if flow == FlowControl.RETURN and instruction.code in (Code.RETND, Code.RETND_IMM16):
            return [f"RET({instruction.immediate(0) if instruction.op_count else 0});"]
        if flow == FlowControl.UNCONDITIONAL_BRANCH and instruction.op0_kind == OpKind.NEAR_BRANCH32:
            return [self.transfer(instruction.near_branch_target, entry)]
        if flow == FlowControl.CONDITIONAL_BRANCH and instruction.op0_kind == OpKind.NEAR_BRANCH32:
            test = "!cpu.ecx" if mnemonic == M.JECXZ else None
            test = self.condition(instruction) if instruction.condition_code else test
            if test:
                return [f"if ({test}) {self.transfer(instruction.near_branch_target, entry)}"]
        if flow == FlowControl.INDIRECT_BRANCH and width_of(instruction) == 32:
            table = self.program.tables.get(address)
            if table:
                cases = " ".join(f"case {index}: {self.transfer(target, entry)}" for index, target in enumerate(table))
                return [f"switch ({REGISTERS[instruction.memory_index]}) {{ {cases} }}", f"TRAP(0x{address:x}u);"]
            if self.program.import_name(instruction):
                return [f"TAIL({self.imports}[{self.slots[instruction.memory_displacement]}]);"]
            return [f"JUMP_INDIRECT({read(instruction, 0)});"]
        raise Unsupported(self.formatter.format(instruction))

    def function(self, entry):
        """The C++ definition of the function at `entry`."""
        program = self.program
        addresses = program.functions[entry]
        self.used_labels = set()
        body = []
        sites = []
        calls = False
        for position, address in enumerate(addresses):
            instruction = program.instructions[address]
            flow = instruction.flow_control
            try:
                lines = self.instruction(instruction, entry)
            except Unsupported as error:
                self.unsupported[str(error)] += 1
                lines, flow = [f"TRAP(0x{address:x}u);"], FlowControl.EXCEPTION
            if address in self.hooks:
                lines.insert(0, f"HOOK(0x{address:x}u);")
            calls = calls or flow in (FlowControl.CALL, FlowControl.INDIRECT_CALL)
            falls = flow in (FlowControl.NEXT, FlowControl.CONDITIONAL_BRANCH, FlowControl.CALL,
                             FlowControl.INDIRECT_CALL)
            following = instruction.next_ip
            if falls and following not in program.instructions:
                lines.append(f"TRAP(0x{address:x}u);")
            elif falls:
                if flow in (FlowControl.CALL, FlowControl.INDIRECT_CALL):
                    sites.append(f"case 0x{following:x}u: {self.transfer(following, entry)}")
                if position + 1 == len(addresses) or addresses[position + 1] != following or \
                        program.owner[following] != entry:
                    lines.append(self.transfer(following, entry))
            elif flow not in (FlowControl.UNCONDITIONAL_BRANCH, FlowControl.INDIRECT_BRANCH,
                              FlowControl.RETURN, FlowControl.EXCEPTION):
                lines.append(f"TRAP(0x{address:x}u);")
            braces = any(line.startswith(("u8 ", "u16 ", "u32 ", "s64 ", "auto ")) for line in lines)
            text = " ".join(lines)
            body.append((address, f"{{ {text} }}" if braces else text or ";",
                         self.formatter.format(instruction)))
        saved = [f"{name}{address:x}" for address in addresses for name in sorted(self.saved.get(address, ()))]
        output = [f"u32 sub_{entry:x}(u8* __restrict m, u32 esp) {{"]
        if calls:
            output.append("    u32 frame = esp;")
        for start in range(0, len(saved), 12):
            output.append(f"    u32 {', '.join(saved[start:start + 12])};")
        if addresses[0] != entry:
            output.append(f"    {self.transfer(entry, entry)}")
        for address, text, comment in body:
            label = f"L{address:x}: " if address in self.used_labels else ""
            output.append(f"    {label}{text} // {address:x}: {comment}")
        if calls:
            # A callee came back somewhere other than its call site: resume here if the target
            # is one of this frame's call sites, else keep unwinding.
            output.append("unwind:")
            output.append("    if (esp <= frame) switch (cpu.eip) { " + " ".join(sites) + " default: break; }")
            output.append("    return esp;")
        output.append("}")
        return "\n".join(output)

/*
 * Alias.c — memory aliases of the back end (mweppc src/compiler/optimizer/Alias.c), partial port.
 *
 * Ported so far (from the exe; a simpler version of mweppc's): initialize_aliases, hash_alias,
 * create_alias, the hash lookup, make_alias, make_alias_set and add_alias_member. Ours keeps no
 * worst-case object, adds every new alias to worst_case_ptr, does not reset the offset of a
 * whole-object alias and never checks for duplicate members.
 *
 * Alias (0x2e bytes): next +0, hashNext +4, children +8, parents +0xc, object +0x10, offset +0x14,
 * size +0x18, index +0x28, type +0x2c (Object 0, ObjectPart 1, Set 2). AliasMember (0x10 bytes):
 * nextParent +0, nextChild +4, parent +8, child +0xc. aliases 0x5535d0, n_aliases 0x5535cc,
 * 0x5535c8, alias_hash 0x552634 (997), worst_case_ptr 0x552630.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void *, lalloc_al, 0x433320, (SInt32 size));
ORIG_FN(void, memclrw_al, 0x4334d0, (void *ptr, SInt32 size));

#define aliases ORIG_VAR(UInt8 *, 0x5535d0)
#define n_aliases ORIG_VAR(SInt32, 0x5535cc)
#define alias_hash ((UInt8 **)0x552634)
#define worst_case_ptr ORIG_VAR(UInt8 *, 0x552630)

UInt8 *make_alias_set(void);

void initialize_aliases(void) {
    int i;

    aliases = 0;
    n_aliases = 0;
    *(SInt32 *)0x5535c8 = 0;
    for (i = 0; i < 997; i++)
        alias_hash[i] = 0;
    worst_case_ptr = make_alias_set();
}
HOOK(0x4a5660, initialize_aliases);

/* Object: name +8 (HashNameNode hashval (short) +8) */
UInt32 hash_alias(UInt8 *object, SInt32 offset, SInt32 size) {
    UInt32 h = (UInt32)((SInt32)F(F(object, 8, UInt8 *), 8, SInt16) * offset * size);

    return h % 997;
}
HOOK(0x4a56a0, hash_alias);

UInt8 *create_alias(UInt8 type, UInt8 *object, SInt32 offset, SInt32 size, Bool8 addToHash) {
    UInt8 *alias = lalloc_al(0x2e);
    UInt32 h;

    memclrw_al(alias, 0x2e);
    alias[0x2c] = type;
    F(alias, 0x28, SInt32) = n_aliases++;
    F(alias, 0, UInt8 *) = aliases;
    aliases = alias;
    F(alias, 0x10, UInt8 *) = object;
    F(alias, 0x14, SInt32) = offset;
    F(alias, 0x18, SInt32) = size;
    if (addToHash) {
        h = hash_alias(object, offset, size);
        F(alias, 4, UInt8 *) = alias_hash[h];
        alias_hash[h] = alias;
    }
    return alias;
}
HOOK(0x4a56e0, create_alias);

UInt8 *lookup_alias(UInt8 *object, SInt32 offset, SInt32 size) {
    UInt8 *alias;

    for (alias = alias_hash[hash_alias(object, offset, size)]; alias; alias = F(alias, 4, UInt8 *)) {
        if (F(alias, 0x10, UInt8 *) == object && F(alias, 0x14, SInt32) == offset && F(alias, 0x18, SInt32) == size)
            return alias;
    }
    return 0;
}
HOOK(0x4a5770, lookup_alias);

UInt8 *add_alias_member(UInt8 *parent, UInt8 *child) {
    UInt8 *member = lalloc_al(0x10);

    F(member, 8, UInt8 *) = parent;
    F(member, 0xc, UInt8 *) = child;
    F(member, 0, UInt8 *) = F(parent, 0xc, UInt8 *);
    F(parent, 0xc, UInt8 *) = member;
    F(member, 4, UInt8 *) = F(child, 8, UInt8 *);
    F(child, 8, UInt8 *) = member;
    return member;
}
HOOK(0x4a5880, add_alias_member);

/* Object: type +0xc (size +2) */
UInt8 *make_alias(UInt8 *object, SInt32 offset, SInt32 size) {
    UInt8 *alias, *whole;
    SInt32 objsize;

    if (!object)
        return 0;
    if (!size)
        size = F(F(object, 0xc, UInt8 *), 2, SInt32);
    alias = lookup_alias(object, offset, size);
    if (alias)
        return alias;
    objsize = F(F(object, 0xc, UInt8 *), 2, SInt32);
    if (offset > 0 || size != objsize) {
        whole = make_alias(object, 0, objsize);
        alias = create_alias(1, object, offset, size, 1);
        add_alias_member(whole, alias);
    } else {
        alias = create_alias(0, object, offset, size, 1);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_ALIAS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_ALIAS */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "ALIAS@"); /* marks runs */
#endif
    add_alias_member(worst_case_ptr, alias);
    return alias;
}
HOOK(0x4a57c0, make_alias);

UInt8 *make_alias_set(void) {
    return create_alias(2, 0, 0, 0, 0);
}
HOOK(0x4a5860, make_alias_set);

/* gives every alias a fresh value number (+0x1c, from 0x57cad8) and clears +0x20 */
void alias_number_values(void) {
    UInt8 *a;

    for (a = aliases; a; a = F(a, 0, UInt8 *)) {
        F(a, 0x1c, SInt32) = (*(SInt32 *)0x57cad8)++;
        F(a, 0x20, SInt32) = 0;
    }
}
HOOK(0x4a5b30, alias_number_values);

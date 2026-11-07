/*
 * CLLicenses.c — FlexLM license checkout/checkin bookkeeping of the driver.
 *
 * Port of mweppc's src/driver/CLLicenses.c (CC0, github.com/doldecomp/mweppc). All 10 functions are
 * byte-identical between their GameCube 2.0 compiler and ours. Globals and layout from our code:
 * license_slots (32 x {handle, cookie}) 0x54be20, license_id_counter 0x54be18, license_slot_count
 * 0x54be1c, vendorcode 0x54bf20 (0x28 bytes), licenseFile 0x57b6a8, clState.programSpec 0x57b154.
 * The lp_* calls go through the exe's import thunks (to LMGR326B.dll, which forwards to the real
 * FlexLM DLL). Quirk kept from the source: License_AutoCheckin passes negative cookies to
 * License_Checkin, which ignores them.
 */
#include "orig.h"

typedef struct LicenseSlot {
    void *handle;
    SInt32 cookie;
} LicenseSlot;

#define license_slots ((LicenseSlot *)0x54be20)
#define license_id_counter ORIG_VAR(int, 0x54be18)
#define license_slot_count ORIG_VAR(int, 0x54be1c)
#define vendorcode ORIG_VAR(UInt8 *, 0x54bf20)
#define licenseFile ORIG_VAR(char *, 0x57b6a8)
#define programSpec ((void *)0x57b154) /* clState.programSpec (its .path is at +0) */
#define OSSPEC_SIZE 0x204
#define PATHSPEC_SIZE 0x104

ORIG_FN(void, CLInternalError, 0x408c80, (const char *file, int line, const char *fmt, ...));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(char *, orig_strcat, 0x404f50, (char *d, const char *s));
ORIG_FN(char *, orig_strncpy, 0x404f20, (char *d, const char *s, UInt32 n));
ORIG_FN(char *, orig_getenv, 0x406850, (const char *name));
ORIG_FN_STDCALL(void *, xmalloc, 0x4176a0, (const char *what, int size));
ORIG_FN_STDCALL(int, OS_MakeFileSpec, 0x412d30, (const char *path, void *spec));
ORIG_FN_STDCALL(int, OS_Status, 0x411e20, (const void *spec));
ORIG_FN_STDCALL(int, OS_ResolveLink, 0x4132c0, (const void *link, void *target));
ORIG_FN_STDCALL(char *, OS_PathSpecToString, 0x413070, (const void *path, char *buf, int size));
ORIG_FN(int, lp_checkout, 0x42851c,
        (void *code, int policy, const char *feature, const char *version, int nlic, const char *path,
         void **handle));
ORIG_FN(void, lp_checkin, 0x428522, (void *handle));
ORIG_FN(const char *, lp_errstring, 0x428528, (void *handle));

void License_AutoCheckin(void);
void License_Checkin(SInt32 cookie);

int allocate_license_slot(void *handle, int autoCheckin) {
    int slot;
    for (slot = 0; slot < license_slot_count; slot++)
        if (license_slots[slot].cookie == 0)
            break;
    if (slot >= 32) {
        License_AutoCheckin();
        slot = license_slot_count;
    }
    if (slot < 32) {
        license_id_counter++;
        license_slots[slot].handle = handle;
        license_slots[slot].cookie = autoCheckin ? -license_id_counter : license_id_counter;
        if (slot >= license_slot_count)
            license_slot_count++;
        return license_id_counter;
    }
    CLInternalError("CLLicenses.c", 97, "Out of license space");
    return 0;
}
HOOK(0x4187a0, allocate_license_slot);

int find_license(SInt32 cookie, void **handle) {
    int index;
    for (index = 0; index < license_slot_count; ++index) {
        if (cookie == license_slots[index].cookie || cookie == -license_slots[index].cookie) {
            *handle = license_slots[index].handle;
            return index;
        }
    }
    CLInternalError("CLLicenses.c", 117, "Searched license not found");
    return -1;
}
HOOK(0x418840, find_license);

int get_license_slot_values(int index, void **handle, SInt32 *cookie) {
    if (index >= 0 && index < license_slot_count && license_slots[index].cookie != 0) {
        *handle = license_slots[index].handle;
        *cookie = license_slots[index].cookie;
        return 1;
    }
    return 0;
}
HOOK(0x4188a0, get_license_slot_values);

int delete_license(int index) {
    if (index >= 0 && index < license_slot_count) {
        license_slots[index].cookie = 0;
        license_slots[index].handle = 0;
        if (index + 1 == license_slot_count)
            for (; index >= 0 && license_slots[index].cookie == 0; --index)
                --license_slot_count;
        return 1;
    }
    CLInternalError("CLLicenses.c", 154, "Deleted license not valid");
    return 0;
}
HOOK(0x4188f0, delete_license);

__attribute__((noinline)) int get_license_slot_count(void) { return license_slot_count; }
HOOK(0x418960, get_license_slot_count);

void License_Initialize(void) {
    license_slot_count = 0;
    vendorcode = 0;
}
HOOK(0x418970, License_Initialize);

void License_Terminate(void) {
    int index, i;
    void *handle;
    SInt32 cookie;
    License_AutoCheckin();
    for (index = 0; index < get_license_slot_count(); index++)
        if (get_license_slot_values(index, &handle, &cookie) && cookie > 0)
            License_Checkin(cookie);
    if (vendorcode) {
        for (i = 0; i < 0x28; i++) /* rep stosd */
            vendorcode[i] = 0;
        vendorcode = 0;
    }
}
HOOK(0x418990, License_Terminate);

SInt32 License_Checkout(const char *featureName, const char *licenseVersion, int autoCheckin, char *errorBuf) {
    UInt8 alternatePath[OSSPEC_SIZE];
    char licensePath[256];
    struct {
        void *code;
        const char *vendor;
    } codeHandle;
    void *licenseHandle = 0;
    int result = 0, status;
    char *environmentPath;

    licensePath[0] = 0;
    if (licenseFile)
        orig_strcpy(licensePath, licenseFile);
    if (!licensePath[0]) {
        environmentPath = orig_getenv("LM_LICENSE_FILE");
        if (environmentPath) {
            orig_strcpy(licensePath, environmentPath);
            if (OS_MakeFileSpec(licensePath, alternatePath) != 0 || OS_Status(alternatePath) != 0) {
                if (OS_ResolveLink(programSpec, alternatePath) == 0) {
                    OS_PathSpecToString(alternatePath, licensePath, PATHSPEC_SIZE);
                    orig_strcat(licensePath, "license.dat");
                } else {
                    licensePath[0] = 0;
                }
            }
        }
    }
    if (!licensePath[0]) {
        OS_PathSpecToString(programSpec, licensePath, PATHSPEC_SIZE);
        orig_strcat(licensePath, "license.dat");
        if (OS_MakeFileSpec(licensePath, alternatePath) != 0 || OS_Status(alternatePath) != 0) {
            if (OS_ResolveLink(programSpec, alternatePath) == 0) {
                OS_PathSpecToString(alternatePath, licensePath, PATHSPEC_SIZE);
                orig_strcat(licensePath, "license.dat");
            } else {
                licensePath[0] = 0;
            }
        }
    }
    if (!licensePath[0])
        orig_strcpy(licensePath, "license.dat");
    if (!vendorcode) {
        UInt8 *v = vendorcode = xmalloc(0, 0x28);
        *(UInt16 *)(v + 0x00) = 4;          /* type */
        *(UInt32 *)(v + 0x04) = 0x4cf2d709; /* data[0] */
        *(UInt32 *)(v + 0x08) = 0xd64a5128; /* data[1] */
        *(UInt32 *)(v + 0x10) = 0x463ed9b0; /* keys[1] */
        *(UInt32 *)(v + 0x0c) = 0x745ce35b; /* keys[0] */
        *(UInt32 *)(v + 0x18) = 0xa610dfa5; /* keys[3] */
        *(UInt32 *)(v + 0x14) = 0xc304db20; /* keys[2] */
        *(UInt16 *)(v + 0x1c) = 6;          /* flexlm_version */
        *(UInt16 *)(v + 0x1e) = 1;          /* flexlm_revision */
        v[0x20] = 0;                        /* flexlm_patch */
        v[0x21] = 0;
        orig_strncpy((char *)v + 0x22, "06.0", 5); /* behavior_ver */
    }
    codeHandle.code = vendorcode;
    codeHandle.vendor = "metrowks";
    status = lp_checkout(&codeHandle, 0x101, featureName, licenseVersion, 1, licensePath, &licenseHandle);
    orig_strcpy(errorBuf, "No failure");
    if (status == 0) {
        result = allocate_license_slot(licenseHandle, autoCheckin);
        if (result == 0) {
            orig_strcpy(errorBuf, "Memory error:  Could not store license cookie");
            lp_checkin(licenseHandle);
        }
    } else {
        orig_strcpy(errorBuf, lp_errstring(licenseHandle));
        lp_checkin(licenseHandle);
    }
#ifdef RECOMP_NEGATIVE_CONTROL_CLLICENSES /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CLLICENSES */
    return 0;
#endif
    return result;
}
HOOK(0x418a00, License_Checkout);

void License_Checkin(SInt32 cookie) {
    int index;
    void *handle;
    if (0 < cookie) {
        index = find_license(cookie, &handle);
        if (index >= 0) {
            delete_license(index);
            lp_checkin(handle);
        }
    }
}
HOOK(0x418d10, License_Checkin);

void License_AutoCheckin(void) {
    int index;
    void *handle;
    SInt32 cookie;
    for (index = 0; index < get_license_slot_count(); index++)
        if (get_license_slot_values(index, &handle, &cookie) && cookie < 0)
            License_Checkin(cookie);
}
HOOK(0x418d50, License_AutoCheckin);

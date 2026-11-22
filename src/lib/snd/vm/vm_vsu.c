/* Voice-manager VAB and program setup (SpuVmVSetUp). */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_vm_int.h"
#include "vm_vsu.h"

long SpuVmVSetUp(short vabId, short prog)
{
    if (vabId < 0 || vabId >= SVM_VAB_MAX || _svm_vab_used[vabId] != SVM_VAB_ACTIVE || prog >= kMaxPrograms) {
        return -1;
    }
    _svm_vh = _svm_vab_vh[vabId];
    _svm_pg = _svm_vab_pg[vabId];
    _svm_tn = _svm_vab_tn[vabId];
    _svm_cur.vab_id = vabId;
    _svm_cur.prog = prog;
    _svm_cur.prog_index = _svm_pg[prog].reserved1;
    return 0;
}

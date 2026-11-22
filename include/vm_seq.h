#ifndef VM_SEQ_H
#define VM_SEQ_H

short SpuVmSetSeqVol(short seq_sep_no, unsigned short voll, unsigned short volr, short update);
short SpuVmGetSeqVol(short seq_sep_no, unsigned short *voll, unsigned short *volr);
short SpuVmGetSeqLVol(short seq_sep_no);
short SpuVmGetSeqRVol(short seq_sep_no);
void SpuVmSeqKeyOff(short seq_sep_no);

#endif

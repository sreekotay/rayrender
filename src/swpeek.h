#ifndef RAYRENDER_SWPEEK_H
#define RAYRENDER_SWPEEK_H

#ifndef RAYRENDER_GPU
void *swGetColorBuffer(int *width, int *height);
#ifdef RAYRENDER_IMPL_CC
void swSetAdaptiveAffine(_Bool enable);
_Bool swGetAdaptiveAffine(void);
void swSetBinSize(int w, int h);
void swGetBinSize(int *w, int *h);
void swSetSeq(_Bool enable);
_Bool swGetSeq(void);
#endif
#endif

#endif

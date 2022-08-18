#ifndef _MATH_H
#define _MATH_H

#ifdef __cplusplus
extern "C" {
#endif

double      fmod(double, double);
double      frexp(double, int *);
double      scalbn(double, int);
double      ldexp(double, int);

#ifdef __cplusplus
}
#endif

#endif

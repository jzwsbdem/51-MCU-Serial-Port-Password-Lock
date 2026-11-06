#ifndef MATRIXKEY_H
#define MATRIXKEY_H

extern unsigned char isMatrixKeyRegistered;
extern volatile unsigned char globalMatrixKey; 

void MatrixKeyRegister();
unsigned char MatrixKeyGet();
void MatrixKeyScan();

#endif
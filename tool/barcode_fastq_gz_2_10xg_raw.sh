#!/bin/bash

awk -F "BX:Z:" '{ if(FNR%4==1){if($2!="") barcode=substr($2,0,16);else barcode="NNNNNNNNNNNNNNNN"} if(FNR%8==1) printf("%s 1:N:0:0\n", $1); else if(FNR%8==2) printf("%sNNNNNNN%s\n",barcode,$1);else if (FNR%8==4) printf("KKKKKKKKKKKKKKKKKKKKKKK%s\n",$1); else if (FNR%8==5) printf("%s 3:N:0:0\n", $1); else print $1; }' $1

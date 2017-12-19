/************************************************************************
 *
 *  ctype.c
 *
 *  Character conversion functions
 *
 * ######################################################################
 *
 * mips_start_of_legal_notice
 * 
 * Copyright (c) 2003 MIPS Technologies, Inc. All rights reserved.
 *
 *
 * Unpublished rights (if any) reserved under the copyright laws of the
 * United States of America and other countries.
 *
 * This code is proprietary to MIPS Technologies, Inc. ("MIPS
 * Technologies"). Any copying, reproducing, modifying or use of this code
 * (in whole or in part) that is not expressly permitted in writing by MIPS
 * Technologies or an authorized third party is strictly prohibited. At a
 * minimum, this code is protected under unfair competition and copyright
 * laws. Violations thereof may result in criminal penalties and fines.
 *
 * MIPS Technologies reserves the right to change this code to improve
 * function, design or otherwise. MIPS Technologies does not assume any
 * liability arising out of the application or use of this code, or of any
 * error or omission in such code. Any warranties, whether express,
 * statutory, implied or otherwise, including but not limited to the implied
 * warranties of merchantability or fitness for a particular purpose, are
 * excluded. Except as expressly provided in any written license agreement
 * from MIPS Technologies or an authorized third party, the furnishing of
 * this code does not give recipient any license to any intellectual
 * property rights, including any patent rights, that cover this code.
 *
 * This code shall not be exported or transferred for the purpose of
 * reexporting in violation of any U.S. or non-U.S. regulation, treaty,
 * Executive Order, law, statute, amendment or supplement thereto.
 *
 * This code constitutes one or more of the following: commercial computer
 * software, commercial computer software documentation or other commercial
 * items. If the user of this code, or any related documentation of any
 * kind, including related technical data or manuals, is an agency,
 * department, or other entity of the United States government
 * ("Government"), the use, duplication, reproduction, release,
 * modification, disclosure, or transfer of this code, or any related
 * documentation of any kind, is restricted in accordance with Federal
 * Acquisition Regulation 12.212 for civilian agencies and Defense Federal
 * Acquisition Regulation Supplement 227.7202 for military agencies. The use
 * of this code by the Government is further restricted in accordance with
 * the terms of the license agreement(s) and/or applicable contract terms
 * and conditions covering this code from MIPS Technologies or an authorized
 * third party.
 *
 * 
 * mips_end_of_legal_notice
 * 
 *
 ************************************************************************/

#include "dvrboot_inc/ctype.h"


char _ctype[] = {
	/* 0 */	0,
	/* 1 */ _CTR,
	/* 2 */ _CTR,
	/* 3 */ _CTR,
	/* 4 */ _CTR,
	/* 5 */ _CTR,
	/* 6 */ _CTR,
	/* 7 */ _CTR,
	/* 8 */ _CTR,
	/* 9 */ _SPC+_CTR,
	/* 10 */ _SPC+_CTR,
	/* 11 */ _SPC+_CTR,
	/* 12 */ _SPC+_CTR,
	/* 13 */ _SPC+_CTR,
	/* 14 */ _CTR,
	/* 15 */ _CTR,
	/* 16 */ _CTR,
	/* 17 */ _CTR,
	/* 18 */ _CTR,
	/* 19 */ _CTR,
	/* 20 */ _CTR,
	/* 21 */ _CTR,
	/* 22 */ _CTR,
	/* 23 */ _CTR,
	/* 24 */ _CTR,
	/* 25 */ _CTR,
	/* 26 */ _CTR,
	/* 27 */ _CTR,
	/* 28 */ _CTR,
	/* 29 */ _CTR,
	/* 30 */ _CTR,
	/* 31 */ _CTR,
	/* 32 */ _SPC+_BLK,
	/* 33 */ _PUN,
	/* 34 */ _PUN,
	/* 35 */ _PUN,
	/* 36 */ _PUN,
	/* 37 */ _PUN,
	/* 38 */ _PUN,
	/* 39 */ _PUN,
	/* 40 */ _PUN,
	/* 41 */ _PUN,
	/* 42 */ _PUN,
	/* 43 */ _PUN,
	/* 44 */ _PUN,
	/* 45 */ _PUN,
	/* 46 */ _PUN,
	/* 47 */ _PUN,
	/* 48 */ _DIG,
	/* 49 */ _DIG,
	/* 50 */ _DIG,
	/* 51 */ _DIG,
	/* 52 */ _DIG,
	/* 53 */ _DIG,
	/* 54 */ _DIG,
	/* 55 */ _DIG,
	/* 56 */ _DIG,
	/* 57 */ _DIG,
	/* 58 */ _PUN,
	/* 59 */ _PUN,
	/* 60 */ _PUN,
	/* 61 */ _PUN,
	/* 62 */ _PUN,
	/* 63 */ _PUN,
	/* 64 */ _PUN,
	/* 65 */ _UPC+_HEX,
	/* 66 */ _UPC+_HEX,
	/* 67 */ _UPC+_HEX,
	/* 68 */ _UPC+_HEX,
	/* 69 */ _UPC+_HEX,
	/* 70 */ _UPC+_HEX,
	/* 71 */ _UPC,
	/* 72 */ _UPC,
	/* 73 */ _UPC,
	/* 74 */ _UPC,
	/* 75 */ _UPC,
	/* 76 */ _UPC,
	/* 77 */ _UPC,
	/* 78 */ _UPC,
	/* 79 */ _UPC,
	/* 80 */ _UPC,
	/* 81 */ _UPC,
	/* 82 */ _UPC,
	/* 83 */ _UPC,
	/* 84 */ _UPC,
	/* 85 */ _UPC,
	/* 86 */ _UPC,
	/* 87 */ _UPC,
	/* 88 */ _UPC,
	/* 89 */ _UPC,
	/* 90 */ _UPC,
	/* 91 */ _PUN,
	/* 92 */ _PUN,
	/* 93 */ _PUN,
	/* 94 */ _PUN,
	/* 95 */ _PUN,
	/* 96 */ _PUN,
	/* 97 */ _LWR+_HEX,
	/* 98 */ _LWR+_HEX,
	/* 99 */ _LWR+_HEX,
	/* 100 */ _LWR+_HEX,
	/* 101 */ _LWR+_HEX,
	/* 102 */ _LWR+_HEX,
	/* 103 */ _LWR,
	/* 104 */ _LWR,
	/* 105 */ _LWR,
	/* 106 */ _LWR,
	/* 107 */ _LWR,
	/* 108 */ _LWR,
	/* 109 */ _LWR,
	/* 110 */ _LWR,
	/* 111 */ _LWR,
	/* 112 */ _LWR,
	/* 113 */ _LWR,
	/* 114 */ _LWR,
	/* 115 */ _LWR,
	/* 116 */ _LWR,
	/* 117 */ _LWR,
	/* 118 */ _LWR,
	/* 119 */ _LWR,
	/* 120 */ _LWR,
	/* 121 */ _LWR,
	/* 122 */ _LWR,
	/* 123 */ _PUN,
	/* 124 */ _PUN,
	/* 125 */ _PUN,
	/* 126 */ _PUN,
	/* 127 */ _CTR,
	/* 128 */ 0,
	/* 129 */ 0,
	/* 130 */ 0,
	/* 131 */ 0,
	/* 132 */ 0,
	/* 133 */ 0,
	/* 134 */ 0,
	/* 135 */ 0,
	/* 136 */ 0,
	/* 137 */ 0,
	/* 138 */ 0,
	/* 139 */ 0,
	/* 140 */ 0,
	/* 141 */ 0,
	/* 142 */ 0,
	/* 143 */ 0,
	/* 144 */ 0,
	/* 145 */ 0,
	/* 146 */ 0,
	/* 147 */ 0,
	/* 148 */ 0,
	/* 149 */ 0,
	/* 150 */ 0,
	/* 151 */ 0,
	/* 152 */ 0,
	/* 153 */ 0,
	/* 154 */ 0,
	/* 155 */ 0,
	/* 156 */ 0,
	/* 157 */ 0,
	/* 158 */ 0,
	/* 159 */ 0,
	/* 160 */ 0,
	/* 161 */ _PUN,
	/* 162 */ _PUN,
	/* 163 */ _PUN,
	/* 164 */ _PUN,
	/* 165 */ _PUN,
	/* 166 */ _PUN,
	/* 167 */ _PUN,
	/* 168 */ _PUN,
	/* 169 */ _PUN,
	/* 170 */ _PUN,
	/* 171 */ _PUN,
	/* 172 */ _PUN,
	/* 173 */ _PUN,
	/* 174 */ _PUN,
	/* 175 */ _PUN,
	/* 176 */ _PUN,
	/* 177 */ _PUN,
	/* 178 */ _PUN,
	/* 179 */ _PUN,
	/* 180 */ _PUN,
	/* 181 */ _PUN,
	/* 182 */ _PUN,
	/* 183 */ _PUN,
	/* 184 */ _PUN,
	/* 185 */ _PUN,
	/* 186 */ _PUN,
	/* 187 */ _PUN,
	/* 188 */ _PUN,
	/* 189 */ _PUN,
	/* 190 */ _PUN,
	/* 191 */ _PUN,
	/* 192 */ _PUN,
	/* 193 */ _PUN,
	/* 194 */ _PUN,
	/* 195 */ _PUN,
	/* 196 */ _PUN,
	/* 197 */ _PUN,
	/* 198 */ _PUN,
	/* 199 */ _PUN,
	/* 200 */ _PUN,
	/* 201 */ _PUN,
	/* 202 */ _PUN,
	/* 203 */ _PUN,
	/* 204 */ _PUN,
	/* 205 */ _PUN,
	/* 206 */ _PUN,
	/* 207 */ _PUN,
	/* 208 */ _PUN,
	/* 209 */ _PUN,
	/* 210 */ _PUN,
	/* 211 */ _PUN,
	/* 212 */ _PUN,
	/* 213 */ _PUN,
	/* 214 */ _PUN,
	/* 215 */ _PUN,
	/* 216 */ _PUN,
	/* 217 */ _PUN,
	/* 218 */ _PUN,
	/* 219 */ _PUN,
	/* 220 */ _PUN,
	/* 221 */ _PUN,
	/* 222 */ _PUN,
	/* 223 */ _PUN,
	/* 224 */ 0,
	/* 225 */ 0,
	/* 226 */ 0,
	/* 227 */ 0,
	/* 228 */ 0,
	/* 229 */ 0,
	/* 230 */ 0,
	/* 231 */ 0,
	/* 232 */ 0,
	/* 233 */ 0,
	/* 234 */ 0,
	/* 235 */ 0,
	/* 236 */ 0,
	/* 237 */ 0,
	/* 238 */ 0,
	/* 239 */ 0,
	/* 240 */ 0,
	/* 241 */ 0,
	/* 242 */ 0,
	/* 243 */ 0,
	/* 244 */ 0,
	/* 245 */ 0,
	/* 246 */ 0,
	/* 247 */ 0,
	/* 248 */ 0,
	/* 249 */ 0,
	/* 250 */ 0,
	/* 251 */ 0,
	/* 252 */ 0,
	/* 253 */ 0,
	/* 254 */ 0,
	/* 255 */ 0};

// Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the OpenSSL license (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html

// ====================================================================
// Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
// project. The module is, however, dual licensed under OpenSSL and
// CRYPTOGAMS licenses depending on where you obtain it. For further
// details see http://www.openssl.org/~appro/cryptogams/.
//
// Permission to use under GPLv2 terms is granted.
// ====================================================================
//
// SHA256/512 for ARMv8.
//
// Performance in cycles per processed byte and improvement coefficient
// over code generated with "default" compiler:
//
//		SHA256-hw	SHA256(*)	SHA512
// Apple A7	1.97		10.5 (+33%)	6.73 (-1%(**))
// Cortex-A53	2.38		15.5 (+115%)	10.0 (+150%(***))
// Cortex-A57	2.31		11.6 (+86%)	7.51 (+260%(***))
// Denver	2.01		10.5 (+26%)	6.70 (+8%)
// X-Gene			20.0 (+100%)	12.8 (+300%(***))
// Mongoose	2.36		13.0 (+50%)	8.36 (+33%)
//
// (*)	Software SHA256 results are of lesser relevance, presented
//	mostly for informational purposes.
// (**)	The result is a trade-off: it's possible to improve it by
//	10% (or by 1 cycle per round), but at the cost of 20% loss
//	on Cortex-A53 (or by 4 cycles per round).
// (***)	Super-impressive coefficients over gcc-generated code are
//	indication of some compiler "pathology", most notably code
//	generated with -mgeneral-regs-only is significanty faster
//	and the gap is only 40-90%.

.text

.align	6
.type	.LK256,%object
.LK256:
.long	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5
.long	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5
.long	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3
.long	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
.long	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc
.long	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da
.long	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7
.long	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967
.long	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13
.long	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85
.long	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3
.long	0xd192e819,0xd6990624,0xf40e3585,0x106aa070
.long	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5
.long	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3
.long	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
.long	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
.long	0	//terminator
.size	.LK256,.-.LK256

.byte	83,72,65,50,53,54,32,98,108,111,99,107,32,116,
.byte	114,97,110,115,102,111,114,109,32,102,111,114,
.byte	32,65,82,77,118,56,44,32,67,82,89,80,84,79,71,
.byte	65,77,83,32,98,121,32,60,97,112,112,114,111,64,
.byte	111,112,101,110,115,115,108,46,111,114,103,62,0
.align	2
.align	2

.globl	sprd_sha256_block_data_order
.type	sprd_sha256_block_data_order,%function
.align	6
sprd_sha256_block_data_order:
.Lv8_entry:
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0

	ld1	{v0.4s,v1.4s},[x0]
	adr	x3,.LK256

.Loop_hw:
	ld1	{v4.16b,v5.16b,v6.16b,v7.16b},[x1],#64
	sub	x2,x2,#1
	ld1	{v16.4s},[x3],#16
	rev32	v4.16b,v4.16b
	rev32	v5.16b,v5.16b
	rev32	v6.16b,v6.16b
	rev32	v7.16b,v7.16b
	orr	v18.16b,v0.16b,v0.16b		// offload
	orr	v19.16b,v1.16b,v1.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v4.4s
.inst	0x5e2828a4	//sha256su0 v4.16b,v5.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s
.inst	0x5e0760c4	//sha256su1 v4.16b,v6.16b,v7.16b
	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v5.4s
.inst	0x5e2828c5	//sha256su0 v5.16b,v6.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s
.inst	0x5e0460e5	//sha256su1 v5.16b,v7.16b,v4.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v6.4s
.inst	0x5e2828e6	//sha256su0 v6.16b,v7.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s
.inst	0x5e056086	//sha256su1 v6.16b,v4.16b,v5.16b
	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v7.4s
.inst	0x5e282887	//sha256su0 v7.16b,v4.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s
.inst	0x5e0660a7	//sha256su1 v7.16b,v5.16b,v6.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v4.4s
.inst	0x5e2828a4	//sha256su0 v4.16b,v5.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s
.inst	0x5e0760c4	//sha256su1 v4.16b,v6.16b,v7.16b
	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v5.4s
.inst	0x5e2828c5	//sha256su0 v5.16b,v6.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s
.inst	0x5e0460e5	//sha256su1 v5.16b,v7.16b,v4.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v6.4s
.inst	0x5e2828e6	//sha256su0 v6.16b,v7.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s
.inst	0x5e056086	//sha256su1 v6.16b,v4.16b,v5.16b
	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v7.4s
.inst	0x5e282887	//sha256su0 v7.16b,v4.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s
.inst	0x5e0660a7	//sha256su1 v7.16b,v5.16b,v6.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v4.4s
.inst	0x5e2828a4	//sha256su0 v4.16b,v5.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s
.inst	0x5e0760c4	//sha256su1 v4.16b,v6.16b,v7.16b
	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v5.4s
.inst	0x5e2828c5	//sha256su0 v5.16b,v6.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s
.inst	0x5e0460e5	//sha256su1 v5.16b,v7.16b,v4.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v6.4s
.inst	0x5e2828e6	//sha256su0 v6.16b,v7.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s
.inst	0x5e056086	//sha256su1 v6.16b,v4.16b,v5.16b
	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v7.4s
.inst	0x5e282887	//sha256su0 v7.16b,v4.16b
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s
.inst	0x5e0660a7	//sha256su1 v7.16b,v5.16b,v6.16b
	ld1	{v17.4s},[x3],#16
	add	v16.4s,v16.4s,v4.4s
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s

	ld1	{v16.4s},[x3],#16
	add	v17.4s,v17.4s,v5.4s
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s

	ld1	{v17.4s},[x3]
	add	v16.4s,v16.4s,v6.4s
	sub	x3,x3,#64*4-16	// rewind
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e104020	//sha256h v0.16b,v1.16b,v16.4s
.inst	0x5e105041	//sha256h2 v1.16b,v2.16b,v16.4s

	add	v17.4s,v17.4s,v7.4s
	orr	v2.16b,v0.16b,v0.16b
.inst	0x5e114020	//sha256h v0.16b,v1.16b,v17.4s
.inst	0x5e115041	//sha256h2 v1.16b,v2.16b,v17.4s

	add	v0.4s,v0.4s,v18.4s
	add	v1.4s,v1.4s,v19.4s

	cbnz	x2,.Loop_hw

	st1	{v0.4s,v1.4s},[x0]

	ldr	x29,[sp],#16
	ret
.size	sprd_sha256_block_data_order,.-sprd_sha256_block_data_order

num:	f32 5.75
exp: 	i32 4
main:	pushq rbp
		movq rbp, rsp
		subq rsp, 8
		movd [rbp-4],[exp]
		movd [rbp-8],[num]
		pushd [rbp-8]
		pushd [rbp-4]
		call pow	//el return esta en r10
		ftod r10
		movq rsp,rbp
		popq rbp
		ret

pow:	pushq rbp
		movq rbp,rsp
		subq rsp,8
// [rbp+20]=num
// [rbp+16]=exp
		movd [rbp-4],1.0
		movd [rbp-8],0
loop:	cmpd [rbp-8],[rbp+16]
		bge break
		fmul [rbp-4],[rbp+20]
		addd [rbp-8],1

		ba loop
break:	movd r10,[rbp-4]
		movq rsp,rbp
		popq rbp
		ret

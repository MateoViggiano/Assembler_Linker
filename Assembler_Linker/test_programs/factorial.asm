#global main
#extern abc
num:	i64 15
main:	pushq rbp
		movq rbp, rsp
		subq rsp, 16
		movq [rbp-16],[num]
		pushq [rbp-16]
		call fact	//el return esta en r10
		movq rsp,rbp
		popq rbp
		ret

fact:	pushq rbp
		movq rbp,rsp
		subq rsp,16
		movq [rbp-8],1
		movq [rbp-16],1
loop:	cmpq [rbp-16],[rbp+16]
		bgt break 
		mulq [rbp-8],[rbp-16]
		addq [rbp-16],1
		ba loop
break:	movq r10,[rbp-8]
		movq rsp,rbp
		popq rbp
		ret

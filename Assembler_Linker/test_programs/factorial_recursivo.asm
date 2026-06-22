num:	i64 15
main:	pushq rbp
		movq rbp, rsp
		subq rsp, 16
		movq [rbp-16],[num]
		pushq [rbp-16]
		call fact
		movq rsp,rbp
		popq rbp
		ret 

fact:	pushq rbp
		movq rbp,rsp
		subq rsp,16
		cmpq [rbp+16],0
		be base
		pushq [rbp+16]
		subq [rsp],1
		call fact
		popq r0
		mulq r10,[rbp+16]
		ba end
base:	movq r10,1
end:	movq rsp,rbp
		popq rbp
		ret 
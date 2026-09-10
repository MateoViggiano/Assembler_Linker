#global main
main:	pushq rbp
		movq rbp, rsp
		subq rsp, 16
		call prog
		//ftod r10
		movq rsp,rbp
		popq rbp
		ret 

prog:	pushq rbp
		movq rbp, rsp
		subq rsp, 8
		movq [rbp-8],-153
		i64tof [rbp-8]
		movd r10,[rbp-8 ]
		movq rsp,rbp
		popq rbp
		ret 
#include<iostream>
#include<stdint.h>
#include<fstream>
#include<math.h>
#include<viggiano>

using std::cout;
using std::cin;
using std::endl;
using mpv::byte;
using mpv::String;
struct qword{
	union{
		int64_t i64;
		int32_t i32;
		int16_t i16;
		int8_t i8;
		uint64_t u64;
		uint32_t u32;
		uint16_t u16;
		uint8_t u8;
		double f64;
		float f32;
	};
	int64_t& operator=(int64_t x){
		this->i64=x;
		return this->i64;
	}
	operator int64_t(){
		return i64;
	}
};

template<uint8_t n>
uint8_t decode_size(byte opcode){
	opcode.uval<<=n;
	opcode.uval>>=6;
	switch(opcode.uval){
		case 0:return sizeof(int8_t);break;
		case 1:return sizeof(int16_t);break;
		case 2:return sizeof(int32_t);break;
		case 3:return sizeof(int64_t);break;
		default: throw mpv::String("Error at decoding size");
	}
}
#define PC registers[15].u64
#define SP registers[14].u64
#define FP registers[13].u64
class VM{
	public:
	bool lt,gt,eq;
	qword registers[16]={};
	struct Memory{
		uint8_t m[mpv::static_power_v<2,30>]={};
		uint8_t& operator[](size_t index){
			if(index>=sizeof(m)) throw mpv::String("Program attempted to access to unallocated memory at address: ")+mpv::String(mpv::to_str(index))+'\n';
			else return m[index];
		}
	}memory;
	//uint8_t memory[mpv::static_power_v<2,30>]={};// 1gb ram
	VM(const String& file_name){
		std::ifstream executable(file_name.c_str(),std::ios::in | std::ios::binary | std::ios::ate);
		if(executable.fail()){
			std::cout<<"Executable couldn't be loaded\n";
			exit(999);
		}
		size_t prog_size=executable.tellg();
		executable.seekg(0);
		executable.read(reinterpret_cast<char*>(&memory),prog_size);
		executable.close();
		FP=SP=sizeof(memory);// SP starts in an invalid address(because stack is empty), it needs to be decremented in order to point to valid memory. When pushing something into the stack SP gets automatically decremented
	}
	uint8_t* find_address(bool use_base_reg){
		const byte code=memory[++PC];
		bool use_index_reg=code.get<0>(), use_address=code.get<1>();
		uint8_t scaling_factor=decode_size<2>(code);
		uint64_t base_reg=0;
		int64_t index_reg=0,address=0;
		if(use_base_reg && use_index_reg){
			const byte regs=memory[++PC];
			base_reg=registers[regs.get_left_bits<4>()];
			index_reg=registers[regs.get_right_bits<4>()];
		}
		else if(use_base_reg) base_reg=registers[code.get_right_bits<4>()];
		else if(use_index_reg) index_reg=registers[code.get_right_bits<4>()];
		if(use_address){//esta bien asi sin el else
			address=*reinterpret_cast<int64_t*>(&memory[PC+1]);
			PC+=sizeof(int64_t);
		}
		return reinterpret_cast<uint8_t*>(&memory[base_reg+index_reg*scaling_factor+address]);
	}
	enum class OperandType:bool{source,dest};
	template<unsigned char b1,unsigned char b2,OperandType optype>
	mpv::If_t<optype==OperandType::source,const uint8_t*,uint8_t*> get_op(const byte opcode,const uint8_t size){
		if(opcode.get<b1>()==1){//derreference value
			return find_address(opcode.get<b2>());
		}
		else{// use value
			if constexpr(optype==OperandType::source){
				if(opcode.get<b2>()==1){// from register
					return reinterpret_cast<uint8_t*>(&registers[memory[++PC]]);
				}
				else{	// from immediate
					uint8_t* p=&memory[PC+1];
					PC+=size;
					return p;
				}
			}else{	//dest cannot be an immediate, so we assume it is a register
				return reinterpret_cast<uint8_t*>(&registers[memory[++PC]]);
			}
		}
	}
	uint8_t* find_address_alt(mpv::byte opinfo){
		bool use_base_reg=opinfo.get<3>(),use_index_reg=opinfo.get<4>(), use_address=opinfo.get<5>();
		uint8_t scaling_factor=decode_size<6>(opinfo);
		uint64_t base_reg=0;
		int64_t index_reg=0,address=0;
		if(use_base_reg || use_index_reg){
			const byte regs=memory[++PC];
			base_reg=use_base_reg ? registers[regs.get_left_bits<4>()] : 0;
			index_reg=use_index_reg ? registers[regs.get_right_bits<4>()] : 0;
		}
		if(use_address){//esta bien asi sin el else
			address=*reinterpret_cast<int64_t*>(&memory[PC+1]);
			PC+=sizeof(int64_t);
		}
		return reinterpret_cast<uint8_t*>(&memory[base_reg+index_reg*scaling_factor+address]);
	}
	template<OperandType optype>
	mpv::If_t<optype==OperandType::source,const uint8_t*,uint8_t*> get_op_alt(const uint8_t size){//used only for alu and fpu operands
		mpv::byte opinfo=memory[++PC];
		if(opinfo.get<2>()==1){//derreference value
			return find_address_alt(opinfo);
		}
		else{// use value
			if constexpr(optype==OperandType::source){
				if(opinfo.get<3>()==1){// from register
					return reinterpret_cast<uint8_t*>(&registers[opinfo.get_right_bits<4>()]);
				}
				else{	// from immediate
					uint8_t* p=&memory[PC+1];
					PC+=size;
					return p;
				}
			}else{	//dest cannot be an immediate, so we assume it is a register
				return reinterpret_cast<uint8_t*>(&registers[opinfo.get_right_bits<4>()]);
			}
		}
	}
	void mov(){
		const byte opcode=memory[PC];
		uint8_t size=decode_size<2>(opcode);
		uint8_t* op1=get_op<4,6,OperandType::dest>(opcode,size);
		const uint8_t* op2=get_op<5,7,OperandType::source>(opcode,size);
		mpv::copy_overlap_n(op1,op2,size);
		
		PC++;
	}
	void push(){
		const byte opcode=memory[PC];
		uint8_t size=decode_size<4>(opcode);
		const uint8_t* operand=get_op<6,7,OperandType::source>(opcode,size);
		SP-=size;
		mpv::copy_overlap_n(&memory[SP],operand,size);
		PC++;
	}
	void pop(){
		const byte opcode=memory[PC];
		uint8_t size=decode_size<4>(opcode);
		uint8_t* operand=get_op<6,7,OperandType::dest>(opcode,size);
		mpv::copy_overlap_n(operand,&memory[SP],size);
		SP+=size;
		PC++;
	}
	void call(){
		const uint64_t* operand=reinterpret_cast<const uint64_t*>(get_op<6,7,OperandType::source>(memory[PC],sizeof(uint64_t)));
		SP-=sizeof(uint64_t);
		*reinterpret_cast<uint64_t*>(&memory[SP])=PC+1;
		PC=*operand;
	}
	void ret(){
		PC=*reinterpret_cast<uint64_t*>(&memory[SP]);
		SP+=sizeof(uint64_t);
	}
	void lea(){
		const byte opcode=memory[PC];
		uint64_t* op1=&registers[byte(memory[PC+1]).get_left_bits<4>()].u64;
		uint8_t scaling_factor=decode_size<6>(opcode);
		uint64_t base_reg=opcode.get<3>() ? registers[byte(memory[++PC]).get_right_bits<4>()].u64 : 0;
		int64_t index_reg=opcode.get<4>() ? registers[byte(memory[++PC]).get_right_bits<4>()].i64 : 0;
		if(!opcode.get<3>() && !opcode.get<4>()) PC++;
		int64_t address=0;
		if(opcode.get<5>()){
			address=*reinterpret_cast<int64_t*>(&memory[PC+1]);
			PC+=sizeof(int64_t);
		}
		*op1=base_reg+index_reg*scaling_factor+address;
		PC++;
	}
	template<typename T>
	void cmp(const T* op1,const T* op2){
		if(*op1==*op2){
			eq=true;
			lt=false;
			gt=false;
		}else if(*op1<*op2){
			eq=false;
			lt=true;
			gt=false;
		}else{
			eq=false;
			lt=false;
			gt=true;
		}
	}
	template<typename T>
	void alu(){
		const byte operation=memory[PC] & 0b00001111;
		bool bit8=memory[PC+1]&0b10000000;
		T* op1=reinterpret_cast<T*>(get_op_alt<OperandType::dest>(sizeof(T)));
		if(operation>=12){	//these operations opcodes are only 2 bytes long
			switch(operation){
				case 12:*op1=~*op1;break;
				case 13:*op1=-*op1;break;
				case 14:if(bit8==0)	*reinterpret_cast<mpv::double_size_t<mpv::make_signed_t<T>>*>(op1)=*op1;
						else *reinterpret_cast<mpv::double_size_t<mpv::make_unsigned_t<T>>*>(op1)=*op1;
						break;
				default:break;//undefined
			}
		}
		else if(operation==10 or operation==11){
			const uint8_t* op2=get_op_alt<OperandType::source>(sizeof(uint8_t));
			if(operation==10) *reinterpret_cast<mpv::make_unsigned_t<T>*>(op1)>>=*op2;
			else{//operation==11
				if(bit8==0) *reinterpret_cast<mpv::make_unsigned_t<T>*>(op1)>>=*op2;
				else 			  *reinterpret_cast<mpv::make_signed_t<T>*>(op1)>>=*op2;
			}
		}
		else{
			const T* op2=reinterpret_cast<const T*>(get_op_alt<OperandType::source>(sizeof(T)));
			switch(operation){
				case 0:	*op1+=*op2;break;
				case 1:	*op1-=*op2;break;
				case 2: if(bit8==0) *reinterpret_cast<mpv::double_size_t<mpv::make_unsigned_t<T>>*>(op1)=static_cast<mpv::double_size_t<mpv::make_unsigned_t<T>>>(*op1) * (*reinterpret_cast<mpv::make_unsigned_t<const T>*>(op2));
						else 			  *reinterpret_cast<mpv::double_size_t<mpv::make_signed_t<T>>*>(op1)=static_cast<mpv::double_size_t<mpv::make_signed_t<T>>>(*op1) * (*reinterpret_cast<mpv::make_signed_t<const T>*>(op2));
						break;
				case 3: if(bit8==0) *reinterpret_cast<mpv::make_unsigned_t<T>*>(op1)/=*reinterpret_cast<mpv::make_unsigned_t<const T>*>(op2);
						else 			  *reinterpret_cast<mpv::make_signed_t<T>*>(op1)/=*reinterpret_cast<mpv::make_signed_t<const T>*>(op2);
						break;
				case 4:	if(bit8==0) *reinterpret_cast<mpv::make_unsigned_t<T>*>(op1)%=*reinterpret_cast<mpv::make_unsigned_t<const T>*>(op2);
						else 			  *reinterpret_cast<mpv::make_signed_t<T>*>(op1)%=*reinterpret_cast<mpv::make_signed_t<const T>*>(op2);
						break;
				case 5:	*op1&=*op2;break;
				case 6:	*op1|=*op2;break;
				case 7:	*op1^=*op2;break;
				case 8:	if(bit8==0) cmp(reinterpret_cast<mpv::make_unsigned_t<T>*>(op1),reinterpret_cast<mpv::make_unsigned_t<const T>*>(op2));
						else cmp(reinterpret_cast<mpv::make_signed_t<T>*>(op1),reinterpret_cast<mpv::make_signed_t<const T>*>(op2));
						break;
				case 9: if(bit8==0) *reinterpret_cast<mpv::make_unsigned_t<T>*>(op1)=pow(*reinterpret_cast<mpv::make_unsigned_t<T>*>(op1),*reinterpret_cast<mpv::make_unsigned_t<const T>*>(op2));
						else 			  *reinterpret_cast<mpv::make_signed_t<T>*>(op1)=pow(*reinterpret_cast<mpv::make_signed_t<T>*>(op1),*reinterpret_cast<mpv::make_signed_t<const T>*>(op2));
			}
		}	
		PC++;
	}
	mpv::Optional<int64_t> branch(){
		const byte opcode=memory[PC];
		enum:uint8_t{ba=0b00000,blt=0b00100,bgt=0b01000,beq=0b01100,bne=0b10000,ble=0b10100,bge=0b11000};
		uint64_t jmpto; 
		if(opcode.get_right_bits<2>()==0b00000000){	// Relative jump
			uint64_t prevPC=PC;
			jmpto=prevPC+*reinterpret_cast<const int32_t*>(get_op<6,7,OperandType::source>(opcode,sizeof(int32_t)));
		}
		else jmpto=*reinterpret_cast<const uint64_t*>(get_op<6,7,OperandType::source>(opcode,sizeof(uint64_t)));
		switch(opcode.val & 0b00011100){
			case ba : PC=jmpto;break;
			case blt: PC=lt?jmpto:PC+1;break;
			case bgt: PC=gt?jmpto:PC+1;break;
			case beq: PC=eq?jmpto:PC+1;break;
			case bne: PC=!eq?jmpto:PC+1;break;
			case ble: PC=lt || eq?jmpto:PC+1;break;
			case bge: PC=gt || eq?jmpto:PC+1;break;
			default: return jmpto;// En esta caso jmpto no se usa como una direccion sino como un valora para retornar
		}
		return {};
	}
	template<typename Float>
	void fpu(){
		const byte operation=memory[PC] & 0b00000111, x=memory[PC+1];
		Float* op1=reinterpret_cast<Float*>(get_op_alt<OperandType::dest>(sizeof(Float)));
		if(operation<=4){
			const Float* op2=reinterpret_cast<const Float*>(get_op_alt<OperandType::source>(sizeof(Float)));
			switch(operation){
				case 0: *op1+=*op2;break;
				case 1: *op1-=*op2;break;
				case 2: *op1*=*op2;break;
				case 3:	*op1/=*op2;break;
				default:if(x.get<0>()==0) cmp(op1,op2);
						else *op1=pow(*op1,*op2);

			}
		}
		else{
			using OtherFloat=mpv::If_t<sizeof(Float)==4,double,float>;
			switch(operation){
				case 5: *reinterpret_cast<OtherFloat*>(op1)=static_cast<OtherFloat>(*op1);break;
				case 6: switch(x.get_left_bits<2>()){
							case 0: *reinterpret_cast<uint32_t*>(op1)=static_cast<uint32_t>(*op1);break;
							case 1: *reinterpret_cast<int32_t*>(op1)=static_cast<int32_t>(*op1);break;
							case 2: *reinterpret_cast<uint64_t*>(op1)=static_cast<uint64_t>(*op1);break;
							case 3:*reinterpret_cast<int64_t*>(op1)=static_cast<int64_t>(*op1);break;
						}break;
				case 7:	switch(x.get_left_bits<2>()){
							case 0: *op1=*reinterpret_cast<uint32_t*>(op1);break;
							case 1: *op1=*reinterpret_cast<int32_t*>(op1);break;
							case 2: *op1=*reinterpret_cast<uint64_t*>(op1);break;
							case 3:	*op1=*reinterpret_cast<int64_t*>(op1);break;
						}break;
			}
		}
		PC++;
	}
	int64_t execute(){
		while(true){
			byte opcode=memory[PC];
			switch(opcode.get_left_bits<2>()){
				case 0: mov();break;
				case 1: if(opcode.get<2>()==0){//stack
							if(opcode.get<3>()==0) push();
							else pop();
						}
						else{//fpu/lea
							if(opcode.get<3>()==0){//fpu
								if(opcode.get<4>()==0) fpu<float>();
								else fpu<double>();
							}
							else{//subroutines/undefined
								if(opcode.get<4>()==0){//subroutines
									if(opcode.get<5>()==0) call();
									else ret();
								}
								else{//undefined
									
								}
							}
						}break;
				case 2:	switch(opcode.val & 0b00110000){//alu
							case 0b00000000: alu<int8_t>();break;
							case 0b00010000: alu<int16_t>();break;
							case 0b00100000: alu<int32_t>();break;
							case 0b00110000: alu<int64_t>();break;
						}break;
				case 3:	if(opcode.get<2>()==0){//branches
							mpv::Optional<int64_t> ret_val=branch();// Branch function may terminate program's execution
							if(ret_val.has_value()) return ret_val.value();
						}
						else lea();
						break;
			}			
		}
	}
};
#include"vmdbg.hpp"

//using namespace std;
int main(int argc,char** argv){
	String progname;
	bool debug=false;
	if(argc==1){
		cout<<"Enter executable name: ";
		cin>>progname;
		debug=true;
	}
	else if(argc==2){
		progname=argv[1];
	}
	else if(argc==3){
		progname=argv[1];
		if(String(argv[2]).lower_cpy()=="-debug") debug=true;
		else{
			cout<<"Unknown command: "<<argv[2]<<endl;
			return 1;
		}
	}
	else{
		cout<<"More than 2 arguments are not allowed";
		return 1;
	}
	VM& cpu=*(new VM(progname));
	try{
		int64_t ret=cpu.execute();
		cout<<"\nProgram returned: "<<ret<<endl;
		if(debug){
			vmdbg::VM_debug displayer(cpu);
			displayer.start();
		}
	}
	catch(mpv::String msg){
		cout<<msg;
		if(debug){
			vmdbg::VM_debug displayer(cpu);
			displayer.start();
		}
	}
	
}

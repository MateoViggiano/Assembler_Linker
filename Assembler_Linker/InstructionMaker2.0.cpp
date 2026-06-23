uint8_t get_reg_num(const String& s){
	return s.substr(1).parse<uint8_t>();
}
uint8_t get_scaling_factor(const String& s){
	if(!s.is_numeric()) throw "Scaling factor should be an integer(1,2,4 or 8)"_s;
	uint8_t v=s.parse<uint8_t>();
	if(v!=1 && v!=2 && v!=4 && v!=8) throw "Scaling factor can only be 1,2,4 or 8"_s;
	return v;
}
bool is_sign(const String& s){
	return s=="+" || s=="-";
}
template<uint8_t bit>
void encode_size(byte& b,uint8_t size){
	uint8_t x;
	switch(size){
		case 1:	x=0b00000000;break;
		case 2: x=0b01000000;break;
		case 4: x=0b10000000;break;
		case 8: x=0b11000000;break;
		default: throw String("This error should never occur, size should be 1,2,4 or 8");
	}
	x>>=bit;
	b.val|=x;
}
uint8_t get_size(char suffix){
	switch(suffix){
		case 'b':	return 1;
		case 'w': 	return 2;
		case 'd': 	return 4;
		case 'q': 	return 8;
		default:	throw String("This error should never occur");
	}
}
void load_immediate(const void* imptr,Vector<mpv::byte>& bytes,uint8_t size){
	bytes.resize(bytes.size()+size);
	mpv::copy_n(&bytes[bytes.size()-size].uval,reinterpret_cast<const unsigned char*>(imptr),size);
}
struct Immediate{
	bool is_int=false,is_uint=false,is_float=false;
	int64_t integer;
	uint64_t uinteger;
	double floating;
	void parse(const String& s){
		if(s.is_numeric()){
			is_float=is_int=is_uint=true;
			integer=uinteger=s.parse<uint64_t>();
			floating=s.parse<double>();
		}
		else if(s.is_int()){
			is_float=is_int=true;
			integer=s.parse<int64_t>();
			floating=s.parse<double>();
		}
		else{
			is_float=true;
			floating=s.parse<double>();
		}
	}
	void parse_int(const String& s){
		if(!s.is_numeric()) throw "Expected integer"_s;
		is_int=is_uint=true;
		integer=uinteger=s.parse<uint64_t>();
	}
	void parse_uint(const String& s){
		if(!s.is_numeric()) throw "Expected unsigned integer"_s;
		is_uint=true;
		uinteger=s.parse<uint64_t>();
	}
	void neg(){
		is_uint=false;
		integer=-integer;
		floating=-floating;
	}
};
struct Operand{
	Immediate immediate;
	String label;
	const char* str=nullptr;
	bool has_immediate=false,dereference=false;
	int8_t has_label=0,scaling_factor=1,base_register=-1,index_register=-1;
};
template<typename Out>
Out& operator<<(Out& stream,const Operand& op){
	if(op.dereference){
		stream<<'[';
		if(op.base_register!=-1) stream<<'r'<<(int)op.base_register;
		if(op.index_register!=-1) stream<<"+r"<<(int)op.index_register<<'*'<<(int)op.scaling_factor;
		if(op.has_immediate){
			if(op.immediate.floating>=0) stream<<'+';
			if(op.has_label) stream<<"LABEL";
			else stream<<op.immediate.floating;
		}
		stream<<']';
	}else{
		if(op.base_register!=-1) stream<<'r'<<(int)op.base_register;
		else if(op.has_label) stream<<"LABEL";
		else if(op.has_immediate) stream<<op.immediate.floating;

	}
	return stream;
}
struct InstructionMaker{
	const Vector<Token>& tokens;
	Vector<mpv::byte>& instruction;
	Map<String,RelativeAddress>& labels;
	Map<String,Vector<uint64_t>>& extern_labels;
	const uint64_t current_address;
	size_t x;
	InstructionMaker(const Vector<Token>& tokens,Vector<mpv::byte>& instruction,Map<String,RelativeAddress>& labels,Map<String,Vector<uint64_t>>& extern_labels,uint64_t current_address):tokens(tokens),instruction(instruction),labels(labels),extern_labels(extern_labels),current_address(current_address),x(tokens[0].type==TokType::label_decl ? 1 : 0){}
	void make_instruction(){
		if(tokens[x].type!=TokType::mnemonic) throw String("Expected mnemonic");
		else if(mnemonics.mov.contains(tokens[x])) mov();
		else if(mnemonics.alu.contains(tokens[x])) alu();
		else if(mnemonics.int_to_float.contains(tokens[x])) itof();
		else if(mnemonics.fpu.contains(tokens[x])) fpu();
		else if(mnemonics.stack.contains(tokens[x])) stack();
		else if(mnemonics.branches.contains(tokens[x])) branches();
		else if(mnemonics.values.contains(tokens[x])) values();
		else if(tokens[x]=="call") call();
		else if(tokens[x]=="ret") ret();
		else if(tokens[x]=="lea") lea();
	}
	void mov(){
		instruction.emplace_back(0b00000000);
		uint8_t size=get_size(tokens[x++].back());
		encode_size<2>(instruction[0],size);
		Operand op1=get_operand();
		if(!op1.dereference && op1.has_immediate) throw "First operand shouldn't be an immediate"_s;
		if(x>=tokens.size() || tokens[x]!=",") throw "Expected second operand"_s;
		x++;
		Operand op2=get_operand();
		if(x<tokens.size()) throw "Only two operands are expected"_s;
		//cout<<op1<<", "<<op2<<endl;
		load_operand<4,6>(op1,size);
		load_operand<5,7>(op2,size);
	}
	void alu(){
		instruction.emplace_back(0b10000000);
		String opname=tokens[x++];
		uint8_t size=get_size(opname.back());
		encode_size<2>(instruction[0],size);
		bool single_operand=mnemonics.single_operand_alu.contains(opname);
		Operand op1=get_operand();
		if(!op1.dereference && op1.has_immediate) throw "First operand shouldn't be an immediate"_s;
		load_operand_alt<false>(op1,size);
		instruction[1].write<0>(mnemonics.signed_alu.contains(opname));
		opname.del_back();
		if(opname=="add") instruction[0].write_right_bits<4>(0);
		else if(opname=="sub") instruction[0].write_right_bits<4>(1);
		else if(opname=="mul" || opname=="umul") instruction[0].write_right_bits<4>(2);
		else if(opname=="div" || opname=="udiv") instruction[0].write_right_bits<4>(3);
		else if(opname=="mod" || opname=="umod") instruction[0].write_right_bits<4>(4);
		else if(opname=="and") instruction[0].write_right_bits<4>(5);
		else if(opname=="or") instruction[0].write_right_bits<4>(6);
		else if(opname=="xor") instruction[0].write_right_bits<4>(7);
		else if(opname=="cmp" || opname=="ucmp") instruction[0].write_right_bits<4>(8);
		else if(opname=="pow" || opname=="upow") instruction[0].write_right_bits<4>(9);
		else if(opname=="ls") instruction[0].write_right_bits<4>(10);
		else if(opname=="rs" || opname=="rsa") instruction[0].write_right_bits<4>(11);
		else if(opname=="not") instruction[0].write_right_bits<4>(12);
		else if(opname=="neg") instruction[0].write_right_bits<4>(13);
		else if(opname=="upc" || opname=="uupc") instruction[0].write_right_bits<4>(14);
		else throw "This error should never occur"_s;
		if(single_operand){
			if(x<tokens.size()) throw "Only one operand is expected"_s;
		}
		else{
			if(x>=tokens.size() || tokens[x]!=",") throw "Expected second operand"_s;
			x++;
			Operand op2=get_operand();
			if(x<tokens.size()) throw "Only two operands are expected"_s;
			load_operand_alt<false>(op2,opname=="ls" || opname=="rs" || opname=="rsa" ? 1 : size);
		}
	}
	void fpu(){
		instruction.emplace_back(0b01100000);
		String opname=tokens[x++];
		uint8_t size=opname[0]=='f' ? 4 : 8;
		instruction[0].write<4>(size==8);
		bool single_operand=mnemonics.single_operand_fpu.contains(opname);
		Operand op1=get_operand();
		if(!op1.dereference && op1.has_immediate) throw "First operand shouldn't be an immediate"_s;
		load_operand_alt<true>(op1,size);
		opname.erase(0,1);
		if(opname=="add") instruction[0].write_right_bits<3>(0);
		else if(opname=="sub") instruction[0].write_right_bits<3>(1);
		else if(opname=="mul") instruction[0].write_right_bits<3>(2);
		else if(opname=="div") instruction[0].write_right_bits<3>(3);
		else if(opname=="cmp") instruction[0].write_right_bits<3>(4);
		else if(opname=="pow"){
			instruction[0].write_right_bits<3>(4);
			instruction[1].write<0>(1);
		}
		else if(opname=="tof" || opname=="tod") instruction[0].write_right_bits<3>(5);
		else if(opname=="toi") instruction[0].write_right_bits<3>(6);
		else throw "This error should never happend"_s;
		if(single_operand){
			if(x<tokens.size()) throw "Only one operand is expected"_s;
		}
		else{
			if(x>=tokens.size() || tokens[x]!=",") throw "Expected second operand"_s;
			x++;
			Operand op2=get_operand();
			if(x<tokens.size()) throw "Only two operands are expected"_s;
			load_operand_alt<true>(op2,size);
		}
	}
	void itof(){
		instruction.emplace_back(0b01100111);
		String opname=tokens[x++];
		uint8_t size=opname.back()=='f' ? 4 : 8;
		opname.erase(opname.size()-3);
		instruction[0].write<4>(size==8);
		Operand op1=get_operand();
		if(!op1.dereference && op1.has_immediate) throw "First operand shouldn't be an immediate"_s;
		load_operand_alt<true>(op1,size);
		if(opname=="u32") instruction[1].write_left_bits<2>(0);
		else if(opname=="i32") instruction[1].write_left_bits<2>(1);
		else if(opname=="u64") instruction[1].write_left_bits<2>(2);
		else if(opname=="i64") instruction[1].write_left_bits<2>(3);
		if(x<tokens.size()) throw "Only one operand is expected"_s;
	}
	void stack(){
		instruction.emplace_back(0b01000000);
		String opname=tokens[x++];
		uint8_t size=get_size(opname.back());
		opname.del_back();
		if(opname=="pop") instruction[0].write<3>(1);
		encode_size<4>(instruction[0],size);
		Operand op=get_operand();
		if(opname=="pop" && !op.dereference && op.has_immediate) throw "Operand shouldn't be an immediate"_s;
		load_operand<6,7>(op,size);
	}
	void branches(){
		instruction.emplace_back(0b11000000);
		if(tokens[x]=="ba") 	  instruction[0].write_num<3,3>(0);
		else if(tokens[x]=="blt") instruction[0].write_num<3,3>(1);
		else if(tokens[x]=="bgt") instruction[0].write_num<3,3>(2);
		else if(tokens[x]=="be")  instruction[0].write_num<3,3>(3);
		else if(tokens[x]=="bne") instruction[0].write_num<3,3>(4);
		else if(tokens[x]=="ble") instruction[0].write_num<3,3>(5);
		else if(tokens[x]=="bge") instruction[0].write_num<3,3>(6);
		else if(tokens[x]=="exit") instruction[0].write_num<3,3>(7);
		x++;
		Operand op=get_operand();
		if(op.has_immediate && !op.immediate.is_int && !op.immediate.is_uint) throw "Branch operand should be an integer"_s;
		if(!op.dereference && op.has_immediate){
			op.immediate.integer-=current_address;
			op.immediate.uinteger-=current_address;
			if(op.has_label==-1) throw "Cannot branch to an extern label"_s;
			op.has_label=0;	// No hace falta marcarlo como label porq voy a usar la direccion relativa al PC
		}
		load_operand<6,7>(op,4);
	}
	void call(){
		instruction.emplace_back(0b01110000);
		x++;
		Operand op=get_operand();
		if(op.has_immediate && !op.immediate.is_int && !op.immediate.is_uint) throw "Branch operand should be an integer"_s;
		if(!op.dereference && op.has_immediate && !op.has_label) throw "Calling an arbitrary address is not allowd, use a label"_s;
		load_operand<6,7>(op,8);
	}
	void ret(){
		instruction.emplace_back(0b01110100);
		if(x+1<tokens.size()) throw "No operand expected after 'ret' instruction"_s;
	}
	void lea(){
		instruction.emplace_back(0b11100000);
		x++;
		Operand op1=get_operand();
		if(op1.dereference || op1.has_immediate) throw "First operand should be a register"_s;
		if(x>=tokens.size() || tokens[x]!=",") throw "Expected second operand"_s;
		x++;
		Operand op2=get_operand();
		if(!op2.dereference) throw "Second operand in 'lea' instruction should always be dereferenced"_s;
		if(x<tokens.size()) throw "Only two operands are expected"_s;
		if(op2.base_register!=-1) instruction[0].write<3>(1);
		if(op2.index_register!=-1) instruction[0].write<4>(1);
		if(op2.has_immediate) instruction[0].write<5>(1);
		encode_size<6>(instruction[0],op2.scaling_factor);
		instruction.emplace_back();
		instruction.back().write_left_bits<4>(op1.base_register);
		if(op2.base_register!=-1 && op2.index_register!=-1){
			instruction.back().write_right_bits<4>(op2.base_register);
			instruction.emplace_back(op2.index_register);
		}
		else if(op2.base_register!=-1) instruction.back().write_right_bits<4>(op2.base_register);
		else if(op2.index_register!=-1) instruction.back().write_right_bits<4>(op2.index_register);
		if(op2.has_immediate){
			if(op2.has_label) load_label(op2);
			else if(op2.immediate.is_int) load_immediate(&op2.immediate.integer,instruction,8);
			else load_immediate(&op2.immediate.uinteger,instruction,8);
		} 
	}
	void values(){
		if(tokens[x]=="reserve"){
			if(x+1<tokens.size() && tokens[x+1].type==TokType::value && tokens[x+1].is_numeric()){
				size_t size=tokens[++x].parse<size_t>();
				instruction.resize(size);
			}else throw "Expected integer after 'reserve'"_s;
		}
		else if(tokens[x]=="str"){x++;
			if(!(x<tokens.size() && tokens[x].type==TokType::string)) throw "Expected a string"_s;
			instruction.resize(tokens[x].size()+1);
			mpv::dflt::strcopy(reinterpret_cast<char*>(&instruction[0]),tokens[x++].c_str());
			//cout<<"strstart<"<<tokens[x]<<">strend"<<endl;
			//if(x<tokens.size()) throw "Expected nothing after end of string"_s;//redundante
		}
		else if(tokens[x]=="init"){x++;
			if(!(x<tokens.size() && tokens[x].type==TokType::value && tokens[x].is_numeric())) throw "Expected unsigned integer after 'init'"_s;
			size_t size=tokens[x++].parse<size_t>();
			if(x<tokens.size() && mnemonics.numtypes.contains(tokens[x])){
				String t=tokens[x++];
				uint8_t tsize=t.substr(1).parse<uint8_t>()/8;
				Operand op=get_operand();
				if(op.dereference || op.base_register!=-1) throw "Expected an immediate"_s;
				if(t[0]=='u'){
					if(!op.immediate.is_uint) throw "Value should be an unsigned integer"_s;
					for(size_t i=0;i<size;i++){
						if(op.has_label) load_label(op);
						else load_immediate(&op.immediate.uinteger,instruction,tsize);
					}
				}
				else if(t[0]=='i'){
					if(!op.immediate.is_int) throw "Value should be an integer"_s;
					for(size_t i=0;i<size;i++)
						load_immediate(&op.immediate.integer,instruction,tsize);
				}
				else if(t=="f32"){
					float immediate=op.immediate.floating;
					for(size_t i=0;i<size;i++)
						load_immediate(&immediate,instruction,tsize);
				}
				else{//t=="f64"
					for(size_t i=0;i<size;i++)
						load_immediate(&op.immediate.floating,instruction,tsize);
				}
			}else throw "Expected numeric type (i8,u32,f32,...) after init size"_s;
		}
		else{
			String t=tokens[x++];
			uint8_t tsize=t.substr(1).parse<uint8_t>()/8;
			Operand op=get_operand();
			if(t[0]=='u'){
				if(op.dereference || op.base_register!=-1 || !op.immediate.is_uint) throw "Expected an unsigned integer"_s;
				else if(op.has_label) load_label(op);
				else load_immediate(&op.immediate.uinteger,instruction,tsize);
				while(x<tokens.size()){
					if(tokens[x++]!=",") throw "Expected ',' before second operand"_s;
					op=get_operand();
					if(op.dereference || op.base_register!=-1 || !op.immediate.is_uint) throw "Expected an unsigned integer"_s;
					else if(op.has_label) load_label(op);
					else load_immediate(&op.immediate.uinteger,instruction,tsize);
				}
			}
			else if(t[0]=='i'){
				if(op.dereference || op.base_register!=-1 || !op.immediate.is_int) throw "Expected an integer"_s;
				load_immediate(&op.immediate.integer,instruction,tsize);
				while(x<tokens.size()){
					if(tokens[x++]!=",") throw "Expected ',' before second operand"_s;
					op=get_operand();
					if(op.dereference || op.base_register!=-1 || !op.immediate.is_int) throw "Expected an integer"_s;
					load_immediate(&op.immediate.integer,instruction,tsize);
				}
			}
			else if(t=="f32"){
				if(op.dereference || op.base_register!=-1) throw "Expected a float"_s;
				float immediate=op.immediate.floating;
				load_immediate(&immediate,instruction,tsize);
				while(x<tokens.size()){
					if(tokens[x++]!=",") throw "Expected ',' before second operand"_s;
					op=get_operand();
					if(op.dereference || op.base_register!=-1) throw "Expected a float"_s;
					immediate=op.immediate.floating;
					load_immediate(&immediate,instruction,tsize);
				}
			}
			else{
				if(op.dereference || op.base_register!=-1) throw "Expected a double"_s;
				load_immediate(&op.immediate.floating,instruction,tsize);
				while(x<tokens.size()){
					if(tokens[x++]!=",") throw "Expected ',' before second operand"_s;
					op=get_operand();
					if(op.dereference || op.base_register!=-1) throw "Expected a double"_s;
					load_immediate(&op.immediate.floating,instruction,tsize);
				}
			}
		}
	}
	Operand get_operand(){
		if(x>=tokens.size() || tokens[x]==",") throw "Expected operand"_s;
		Operand op;
		switch(tokens[x].type){
			case TokType::mnemonic: throw "Mnemonic not expected"_s;
			case TokType::string:	throw "String not expected"_s;
			case TokType::label:	op.has_label=1;
									op.has_immediate=true;
									op.immediate.is_uint=true;
									op.label=tokens[x];
									if(labels.contains(tokens[x])) op.immediate.uinteger=labels.at(tokens[x]).address;// Used to calculate branch addresses relative to PC
									else op.has_label=-1; // -1 indicates that the label is extern
									break;
			case TokType::reg:		op.base_register=get_reg_num(tokens[x]);break;
			case TokType::value:	op.has_immediate=true;
									op.immediate.parse(tokens[x]);break;
			case TokType::symbol:	if(tokens[x]=="-"){
										if(x+1<=tokens.size() && tokens[x+1].type==TokType::value){
											op.has_immediate=true;
											op.immediate.parse(tokens[++x]);
											op.immediate.neg();
										}
										else throw "Expected value after sign '-'"_s;
									}
									else if(tokens[x++]=="["){
										op.dereference=true;
										Token last("+");last.type=TokType::symbol;
										bool scaling_factor_used=false;
										while(x<tokens.size() && tokens[x]!="]"){
											switch(tokens[x].type){
												case TokType::mnemonic: throw "Mnemonic not expected"_s;
												case TokType::label:	if(!is_sign(last)) throw "Expected sign ('+' or '-')"_s;
																		op.has_label=true;
																		op.has_immediate=true;
																		op.immediate.is_uint=true;
																		op.label=tokens[x];
																		break;
												case TokType::value:	if(!is_sign(last)) throw "Expected sign ('+' or '-')"_s;
																		op.has_immediate=true;
																		op.immediate.parse_int(tokens[x]);
																		if(last=="-") op.immediate.neg();
																		break;
												case TokType::reg:		if(last!="+") throw "Expected sign '+' before register"_s;
																		else if(op.base_register==-1) op.base_register=get_reg_num(tokens[x]);
																		else if(op.base_register!=-1 && op.index_register==-1) op.index_register=get_reg_num(tokens[x]);
																		else /* if(op.base_register!=-1 && op.index_register!=-1) */ throw "Only two register allowed"_s;
																		
																		break;
												case TokType::symbol:	if(last.type==TokType::symbol) throw "Unexpected symbol '"+tokens[x]+'\'';
																		if(tokens[x]=="*"){
																			if(last.type!=TokType::reg) throw "Expected an index register before '*' symbol";
																			if(scaling_factor_used) throw "Only one index register with scaling factor is allowed"_s;
																			else scaling_factor_used=true;
																			if(x+1<tokens.size() && tokens[x+1].type==TokType::value){
																				if(op.index_register==-1 && op.base_register!=-1){
																					op.index_register=op.base_register;
																					op.base_register=-1;
																				}
																				if(op.index_register!=-1) op.scaling_factor=get_scaling_factor(tokens[++x]);
																				else throw "Only one index register with scaling factor is allowed"_s;
																			}else throw "Expected scaling factor after '*' symbol"_s;
																		}
																		else if(!is_sign(tokens[x])) throw "This is not supported yet"_s;
																		break;
												case TokType::string:	throw "Strings need to be declered with 'str' "_s;
												default: 				throw "This error should never occur"_s;
											}
											last=tokens[x++];
										}
									}
									else throw String("This is not supported yet");
									break;
			default: throw String("This error should never occur");
		}
		x++;
		if(op.dereference && op.base_register==-1 && op.index_register==-1){
			if(!op.has_immediate) throw "Empty '[]' found"_s;
			else if(!op.has_label) throw "Dereferencing an arbitrary address is not allowed, use a label"_s;
		}
		return op;
	}
	void load_label(const Operand& op){
		if(labels.contains(op.label)) labels.at(op.label).references.push_back(current_address+instruction.size());
		else extern_labels.at(op.label).push_back(current_address+instruction.size());
		instruction.resize(instruction.size()+8);
	}
	template<unsigned char b1,unsigned char b2>
	void load_operand(const Operand& op,const uint8_t size){
		if(op.dereference){
			instruction[0].write<b1>(1);
			instruction.emplace_back();
			instruction[0].write<b2>(op.base_register!=-1);
			instruction.back().write<0>(op.index_register!=-1);
			instruction.back().write<1>(op.has_immediate);
			encode_size<2>(instruction.back(),op.scaling_factor);
			if(op.base_register!=-1 && op.index_register!=-1){
				instruction.emplace_back();
				instruction.back().write_left_bits<4>(op.base_register);
				instruction.back().write_right_bits<4>(op.index_register);
			}
			else if(op.base_register!=-1){
				instruction.back().write_right_bits<4>(op.base_register);
			} 
			else if(op.index_register!=-1){
				instruction.back().write_right_bits<4>(op.index_register);
			} 
			if(op.has_immediate){
				if(op.has_label) load_label(op);
				else if(op.immediate.is_int) load_immediate(&op.immediate.integer,instruction,8);
				else load_immediate(&op.immediate.uinteger,instruction,8);
			}
		}
		else{
			if(op.base_register!=-1){
				instruction[0].write<b2>(1);
				instruction.emplace_back(op.base_register);
			}
			else{
				if(!op.immediate.is_int && !op.immediate.is_uint){
					if(size==4){
						float immediate=op.immediate.floating;
						load_immediate(&immediate,instruction,4);
					}
					else if(size==8) load_immediate(&op.immediate.floating,instruction,8);
					else throw "When moving a floating point immediate, size should be 4 or 8"_s;
				}
				else{
					if(op.has_label) load_label(op);
					else if(op.immediate.is_int) load_immediate(&op.immediate.integer,instruction,size);
					else load_immediate(&op.immediate.uinteger,instruction,size);
					
				}
			}
		}
	}
	template<bool floating_point>
	void load_operand_alt(const Operand& op,const uint8_t size){
		instruction.emplace_back();
		if(op.dereference){
			instruction.back().write<2>(1);
			instruction.back().write<3>(op.base_register!=-1);
			instruction.back().write<4>(op.index_register!=-1);	
			instruction.back().write<5>(op.has_immediate);	
			encode_size<2>(instruction.back(),op.scaling_factor);
			if(op.base_register!=-1 || op.index_register!=-1){
				instruction.emplace_back();
				instruction.back().write_left_bits<4>(op.base_register);
				instruction.back().write_right_bits<4>(op.index_register);
			}
			if(op.has_immediate){
				if(op.has_label) load_label(op);
				else if(op.immediate.is_int) load_immediate(&op.immediate.integer,instruction,8);
				else load_immediate(&op.immediate.uinteger,instruction,8);
			}
		}
		else{
			if(op.base_register!=-1){
				instruction.back().write<3>(1);
				instruction.back().write_right_bits<4>(op.base_register);
			}
			else{
				if constexpr(floating_point){
					if(size==4){
						float immediate=op.immediate.floating;
						load_immediate(&immediate,instruction,4);
					}
					else if(size==8) load_immediate(&op.immediate.floating,instruction,8);
					else throw "When moving a floating point immediate, size should be 4 or 8"_s;
				}
				else{
					if(!op.immediate.is_int && !op.immediate.is_uint) throw "Operand should be an integer"_s;
					else if(op.has_label) load_label(op);
					else if(op.immediate.is_int) load_immediate(&op.immediate.integer,instruction,size);
					else load_immediate(&op.immediate.uinteger,instruction,size);
					
				}
			}
		}
	}
};
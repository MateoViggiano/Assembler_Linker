#include<iostream>
#include<fstream>
#include<stdint.h>
#include<viggiano>
#include"Comment_and_spaces_deleter.cpp"
using namespace mpv;
using std::cout;
using std::cin;
using std::endl;
using mpv::byte;
struct Mnemonics:Vector<String>{
	Vector<String> branches={"ba","blt","bgt","be","bne","ble","bge","exit"};
	Vector<String> values={"i8","u8","i16","u16","i32","u32","i64","u64","f32","f64","str","init","reserve"};
	Vector<String> mov={"movb","movw","movd","movq"};
	Vector<String> stack={"pushb","pushw","pushd","pushq","popb","popw","popd","popq"};
	Vector<String> alu={"addb","addw","addd","addq","subb","subw","subd","subq","mulb","mulw","muld","mulq","divb","divw","divd","divq","modb","modw","modd","modq","umulb","umulw","umuld","umulq","udivb","udivw","udivd","udivq","umodb","umodw","umodd","umodq","andb","andw","andd","andq","orb","orw","ord","orq","xorb","xorw","xord","xorq","cmpb","cmpw","cmpd","cmpq","ucmpb","ucmpw","ucmpd","ucmpq","powb","poww","powd","powq","upowb","upoww","upowd","upowq","lsb","lsw","lsd","lsq","rsb","rsw","rsd","rsq","rsab","rsaw","rsad","rsaq","notb","notw","notd","notq","negb","negw","negd","negq","upcb","upcw","upcd","upcq","uupcb","uupcw","uupcd","uupcq"};
	Vector<String> fpu={"ftod","dtof","fadd","dadd","fsub","dsub","fmul","dmul","fdiv","ddiv","fpow","dpow","fcmp","dcmp","ftoi","dtoi"};
	Vector<String> signed_alu={"mulb","mulw","muld","mulq","divb","divw","divd","divq","modb","modw","modd","modq","cmpb","cmpw","cmpd","cmpq","powb","poww","powd","powq","rsab","rsaw","rsad","rsaq","upcb","upcw","upcd","upcq"};
	Vector<String> int_to_float={"u32tof","u32tod","i32tof","i32tod","u64tof","u64tod","i64tof","i64tod"};
	Vector<String> single_operand_fpu={"ftod","dtof","ftoi","dtoi"};
	Vector<String> single_operand_alu={"notb","notw","notd","notq","negb","negw","negd","negq","uupcb","uupcw","uupcd","uupcq","upcb","upcw","upcd","upcq"};
	Vector<String> int_to_float_cast={"u32tof","u32tod","i32tof","i32tod","u64tof","u64tod","i64tof","i64tod"};
	Vector<String> numtypes={"i8","u8","i16","u16","i32","u32","i64","u64","f32","f64"};
	Mnemonics():Vector<String>({"lea","call","ret"}){
		*this+=branches+values+mov+stack+alu+fpu+int_to_float_cast;
	}
}mnemonics;

enum class TokType:char{unknown,label_decl,value,label,org,symbol,string,mnemonic,reg};
struct Token:public String{
	using String::operator=;
	TokType type=TokType::unknown;
	Token()=default;
	explicit Token(const char* s):String(s){}
	Token(const String& s):String(s){}
};

bool is_register(const String& s){
	const char* registers[16]={"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r12","r13","r14","r15"};
	for(const char* r:registers){
		if(s==r) return true;
	}
	return false;
}
struct RelativeAddress{
	uint64_t address;
	Vector<uint64_t> references;
	bool is_global=false;
	RelativeAddress(uint64_t address):address(address){}
};
#include"Tokenizer.cpp"
#include"InstructionMaker2.0.cpp"
struct Line{
	uint64_t address;
	Vector<mpv::byte> instruction;	//operations and values are counted as instructions, labels, comments and preprocessor not
	Vector<Token> tokens;
	Line(Vector<Token>&& tokens):tokens(static_cast<Vector<Token>&&>(tokens)){}

};
Vector<Token> split_into_tokens(const String& s){
	Tokenizer t(s);
	return t.run();
}
void write_labels(std::ostream& out,const Map<String,RelativeAddress>& labels){
	uint32_t s=labels.size();
	out.write(reinterpret_cast<const char*>(&s),sizeof(s));
	for(const MapPair<String,RelativeAddress>& l:labels){
		out<<(l.val.is_global ? 'G' : 'L')<<l.key<<'\n';
		out.write(reinterpret_cast<const char*>(&l.val.address),sizeof(l.val.address));
		s=l.val.references.size();
		out.write(reinterpret_cast<const char*>(&s),sizeof(s));
		for(uint64_t x:l.val.references) out.write(reinterpret_cast<const char*>(&x),sizeof(x));
	}
}
void write_extern_labels(std::ostream& out,const Map<String,Vector<uint64_t>>& labels){
	uint32_t s=labels.size();
	out.write(reinterpret_cast<const char*>(&s),sizeof(s));
	for(const MapPair<String,Vector<uint64_t>>& l:labels){
		out<<l.key<<'\n';
		s=l.val.size();
		out.write(reinterpret_cast<const char*>(&s),sizeof(s));
		for(uint64_t x:l.val) out.write(reinterpret_cast<const char*>(&x),sizeof(x));
	}
}
struct Assembler{
	Vector<Line> lines;
	String input;
	Vector<String> global_labels;
	Map<String,RelativeAddress> labels;
	Map<String,Vector<uint64_t>> extern_labels;
	String fname;
	bool error_state=false;
	public:
	Assembler(const char* fname):fname(fname){
		std::ifstream file(fname,std::ios::binary);
		if(!file.is_open()){
			cout<<'"'<<fname<<"\" could not be opened\n";
			error_state=true;
			return;
		}
		input.readtext(file);
	}
	void create_binary(){
		String ofname=fname.substr(0,fname.find("."))+".vmo";
		std::ofstream file(ofname.c_str(),std::ios::out | std::ios::binary);
		if(!file.is_open()){
			cout<<"Output file could not be opened\n";
			error_state=true;
			return;
		}
		uint64_t highest_address=0;
		for(size_t i=0;i<lines.size();i++)
			if(lines[i].instruction.size()>0 && lines[i].address+lines[i].instruction.size()-1>highest_address) 
				highest_address=lines[i].address+lines[i].instruction.size()-1;
		cout<<"HIGHEST ADDRESS:"<<highest_address<<endl;
		uPtr<char[]> bin(new char[++highest_address]{});
		for(const Line& l:lines)
			copy_n(bin.get()+l.address,reinterpret_cast<const char*>(l.instruction.get_array()),l.instruction.size());
		write_labels(file,labels);
		write_extern_labels(file,extern_labels);
		file.write(reinterpret_cast<char*>(&highest_address),sizeof(highest_address));
		file.write(bin.get(),highest_address);
		file.close();
	}
	void create_instructions(){
		uint64_t current_address=0;
		for(size_t i=0;i<lines.size();i++){
			if(lines[i].tokens.size()==0) continue;
			if(lines[i].tokens[0].type==TokType::org){
				current_address=lines[i].address;
				continue;
			}
			try{
				lines[i].address=current_address;
				if(lines[i].tokens[0].type==TokType::label_decl) labels.at(lines[i].tokens[0]).address=current_address;
				InstructionMaker im(lines[i].tokens,lines[i].instruction,labels,extern_labels,current_address);
				im.make_instruction();
				current_address+=lines[i].instruction.size();

			}catch(String errormsg){
				cout<<"Error<line "<<i+1<<">: "<<errormsg<<endl;
				error_state=true;
			}
		}
		for(size_t i=0;i<lines.size();i++){
			if(lines[i].instruction.size()==5 && lines[i].instruction[0].get_right_bits<2>()==0b00000000 && lines[i].tokens.any([](const Token& t){return t.type==TokType::label;})){ //if(lines[i] is relative jump with label)
				lines[i].instruction.clear();
				InstructionMaker im(lines[i].tokens,lines[i].instruction,labels,extern_labels,lines[i].address);
				im.make_instruction();
			}
		}
		
	}
	void preprocess(){
		using MP=MapPair<String,Vector<Token>>;
		Map<String,Vector<Token>> macros={MP("rip",Vector<Token>{String("r15")}),MP("rsp",Vector<Token>{String("r14")}),MP("rbp",Vector<Token>{String("r13")})};
		for(size_t i=0;i<lines.size();i++){
			if(lines[i].tokens.size()==0) continue;
			try{
				for(size_t j=0;j<lines[i].tokens.size();j++){//tengo que solucionar los casos en los que hay una macro adentro de otra
					if(macros.contains(lines[i].tokens[j])){
						Token current_macro = lines[i].tokens.pop_at(j);
						if(current_macro.type==TokType::label_decl && macros.at(current_macro).size()>1) throw String("Label declaration should be at the beginning of the line");
						lines[i].tokens.join(lines[i].tokens.begin()+j,macros.at(current_macro));
						if(current_macro.type==TokType::label_decl && macros.at(current_macro).size()==1) lines[i].tokens[j].type=TokType::label_decl;
					}
				}
				if(lines[i].tokens[0][0]=='#'){
					if(lines[i].tokens[0]=="#def"){
						if(!lines[i].tokens[1].is_identifier()) throw String("Invalid name for macro");
						else if(macros.contains(lines[i].tokens[1])) throw String("Redeclaring macro: ")+lines[i].tokens[1];
						else if(lines[i].tokens.sublist(lines[i].tokens.begin()+2,lines[i].tokens.end()).any([](const Token& t){return t.type!=TokType::string && t.contains('#');}))throw String("Found '#' inside macro. Defining a macro for a preprocessor command is not allowed");
						else{
							macros.emplace(lines[i].tokens[1],lines[i].tokens.sublist(lines[i].tokens.begin()+2,lines[i].tokens.end()));
							lines[i].tokens.free_storage();
						}
					}
					else if(lines[i].tokens[0]=="#org"){
						if(lines[i].tokens.size()==1) throw String("Missing value after #org command");
						else if(lines[i].tokens.size()>2) throw String("org value should be one integer");
						else{
							lines[i].tokens[0].type=TokType::org;
							if(!lines[i].tokens[1].is_numeric()) throw "Expected unsigned integer after org"_s;
							lines[i].address=lines[i].tokens[1].parse<uint64_t>();
							lines[i].tokens.wipe(lines[i].tokens.begin()+1,lines[i].tokens.end());
						}
					}
					else if(lines[i].tokens[0]=="#extern"){
						if(lines[i].tokens.size()==1) throw String("Expected label name");
						else if(!lines[i].tokens[1].is_identifier()) throw "Invalid label name '"_s+lines[i].tokens[1]+"'";
						else if(lines[i].tokens.size()>2) throw "Expected nothing after extern declaration"_s;
						else if(extern_labels.contains(lines[i].tokens[1])) throw "Redeclaration of extern label: '"_s+lines[i].tokens[1]+'\'';
						else if(global_labels.contains(lines[i].tokens[1])) throw "Label: '"_s+lines[i].tokens[1]+"' already declared global";
						else{
							extern_labels.emplace(lines[i].tokens[1]);
							lines[i].tokens.free_storage();
						}
					}
					else if(lines[i].tokens[0]=="#global"){
						if(lines[i].tokens.size()==1) throw String("Expected label name");
						else if(!lines[i].tokens[1].is_identifier()) throw "Invalid label name '"_s+lines[i].tokens[1]+"'";
						else if(lines[i].tokens.size()>2) throw "Expected nothing after global declaration"_s;
						else if(global_labels.contains(lines[i].tokens[1])) throw "Redeclaration of global label: '"_s+lines[i].tokens[1]+'\'';
						else if(extern_labels.contains(lines[i].tokens[1])) throw "Label: '"_s+lines[i].tokens[1]+"' already declared extern";
						else{
							global_labels.push_back(lines[i].tokens[1]);
							lines[i].tokens.free_storage();
						}
					}
					else throw String("Unknown preprocessor command: '")+lines[i].tokens[0]+String("'");
				}
				else for(const Token& t:lines[i].tokens){
					if(t.type!=TokType::string && t.contains('#')) throw String("Preprocessor commands only allowed at the begininng of a line");
				}
			}catch(String errormsg){
				cout<<"Error<line "<<i+1<<">: "<<errormsg<<endl;
				error_state=true;
			}
		}
	}
	void mark_tokens(){
		for(size_t i=0;i<lines.size();i++){
			if(lines[i].tokens.size()==0) continue;
			try{
				if(lines[i].tokens[0].type==TokType::label_decl){
					if(!lines[i].tokens[0].is_identifier()) throw String("Invalid identifier for label: '")+lines[i].tokens[0]+'\'';
					else if(labels.contains(lines[i].tokens[0])) throw "Redeclaration of label: '"+lines[i].tokens[0]+'\'';
					else if(mnemonics.contains(lines[i].tokens[0])) throw String("Label can not be named as a mnemonic");
					else if(is_register(lines[i].tokens[0])) throw String("Label can not be named as a register");
					else if(extern_labels.contains(lines[i].tokens[0])) throw "Label '"_s+lines[i].tokens[0]+"' already delared extern";
					else labels.emplace(lines[i].tokens[0],0);// el valor de cada etiqueta se calcula al final del programa, por ahora es 0
				}
				if(lines[i].tokens.sublist(lines[i].tokens.begin()+1,lines[i].tokens.end()).any([](const Token& t){return t.type==TokType::label_decl;})) throw String("Label declarations only allowed at the beginning of a line");
				
			}catch(String errormsg){
				cout<<"Error<line "<<i+1<<">: "<<errormsg<<endl;
				error_state=true;
			}
		}
		for(size_t i=0;i<lines.size();i++){
			for(Token& t:lines[i].tokens){
				if(t.type==TokType::unknown){
					if(labels.contains(t) || extern_labels.contains(t)) t.type=TokType::label;
					else if(mnemonics.contains(t)) t.type=TokType::mnemonic;
					else if(is_register(t)) t.type=TokType::reg;
					else if(t.is_float_convertible()) t.type=TokType::value;
					else{
						cout<<"Error: Unknown identifier at line "<<i+1<<endl;
						error_state=true;
					} 					
				}

			}
		}
		for(const String& global:global_labels){
			if(!labels.contains(global)){
				cout<<"Error: Label '"<<global<<"' declared global but not found in file\n";
				error_state=true;
			}
			else labels.at(global).is_global=true;
		}
	}
	void tokenize(){
		Vector<String> input_lines=input.split<Vector>("\n");
		for(size_t i=0;i<input_lines.size();i++){
			input_lines[i].strip();
			try{
				lines.emplace_back(split_into_tokens(input_lines[i]));
			}
			catch(String errormsg){
				cout<<"Error<line "<<i+1<<">: "<<errormsg<<endl;
				error_state=true;
			}
		}
	}
	void delete_comments(){
		Comment_and_spaces_deleter csd(input);
		csd.run();
		if(csd.comment_error){
			error_state=true;
			cout<<"Error: Unterminated block comment at line "<<csd.get_error_line()<<endl;
		}
		else{
			input=static_cast<String&&>(csd.output);
		}
	}
	void run(){
		cout<<fname<<endl;
		delete_comments();
		if(error_state) return;
		tokenize();
		if(error_state) return;
		preprocess();
		if(error_state) return;
		mark_tokens();
		if(error_state) return;
		create_instructions();
		if(error_state) return;
		debug_print();
		cout<<endl;
		debug_print_binary();
		create_binary();
		print_labels();
	}
	void debug_print(){
		for(size_t i=0;i<lines.size();i++){
			cout<<i+1<<':'<<'\t'<<lines[i].address<<'\t'<<lines[i].tokens<<endl;
		}
	}
	void debug_print_binary(){
		for(size_t i=0;i<lines.size();i++){
			cout<<i+1<<':'<<'\t'<<lines[i].address<<'\t';
			for(mpv::byte b:lines[i].instruction){
				cout<<b<<' ';
			}
			cout<<endl;
		}
	}
	void print_labels(){
		puts("Labels: ");
		for(const auto& l:labels){
			cout<<l.key<<": &"<<l.val.address<<' '<<l.val.references<<endl;
		}
		cout<<"Extern labels: "<<extern_labels<<endl;
	}
};
template<typename Out>
Out& operator<<(Out& stream,TokType tt){
	switch(tt){
		case TokType::unknown:		stream<<"<unknown>";break;
		case TokType::label_decl:	stream<<"<label_decl>";break;
		case TokType::value:		stream<<"<value>";break;
		case TokType::label:		stream<<"<label>";break;
		case TokType::org:			stream<<"<org>";break;
		case TokType::symbol:		stream<<"<symbol>";break;
		case TokType::string:		stream<<"<string>";break;
		case TokType::mnemonic:		stream<<"<mnemonic>";break;
		case TokType::reg:			stream<<"<reg>";break;
	}
	return stream;
}
template<typename Out>
Out& operator<<(Out& stream,const Token& t){
	stream<<static_cast<const String&>(t)<<t.type;
	return stream;
}

int main(int argc,char** argv){
	int error_count=0;
	if(argc==1){
		String fname;
		cout<<"Enter file name: ";
		cin>>fname;
		Assembler prog(fname.c_str());
		prog.run();
		return prog.error_state;
	}
	else for(int i=1;i<argc;i++,cout<<"\n\n"){
		Assembler prog(argv[i]);
		if(prog.error_state==true){
			error_count++;
			continue;
		}
		prog.run();
		error_count+=prog.error_state;
	}
	return error_count;
}
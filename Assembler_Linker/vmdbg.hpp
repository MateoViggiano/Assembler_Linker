#pragma once
// Formato: 		type pos
// Formato array: 	type[size] pos
namespace vmdbg{
	using namespace mpv;
	using std::cin;
	using std::cout;
	using std::endl;
	struct VM_debug{
		VM::Memory& memory;
		qword* registers;
		VM_debug(VM& vm):memory(vm.memory),registers(vm.registers){}
		
		template<typename T>
		void display_reg(size_t number){
			using U=	If_t<is_same_v<T,unsigned char>,
							unsigned short,
							If_t<is_same_v<T,signed char>,
							signed short,
							T
							>
						>;
			cout<<(U)*reinterpret_cast<T*>(&registers[number])<<endl;
		}
		
		template<typename T>
		void display(size_t address,Optional<size_t> size){
			using U=	If_t<is_same_v<T,unsigned char>,
							unsigned short,
							If_t<is_same_v<T,signed char>,
							signed short,
							T
							>
						>;
			if(!size.has_value()) cout<<(U)*reinterpret_cast<T*>(&memory[address])<<endl;
			else display_array<T,U>(address,size.value());
		}
		template<typename T,typename U>
		void display_array(size_t address,size_t size){
			cout<<'{';
			for(auto i:Range<size_t>(size)){
				cout<<(U)*reinterpret_cast<T*>(&memory[address+i*sizeof(T)]);
				if(i<size-1) cout<<", ";
			}
			cout<<'}'<<endl;
		}
	
		void start(){
			while(true){
			String input;
				cin>>input;
				input.noExtraSpaces();
				input.strip();
				input.lower();
				if(input=="exit") return;
				if(input.startswith("r: ")){
					input.erase(0,3);
					Parser parser(input);
					String type=parser.get_type();
					size_t reg_number=parser.get_address();
					if(type=="char") display_reg<char>(reg_number);
					else if(type=="u8") display_reg<unsigned char>(reg_number);
					else if(type=="i8") display_reg<signed char>(reg_number);
					else if(type=="u16") display_reg<unsigned short>(reg_number);
					else if(type=="i16") display_reg<signed short>(reg_number);
					else if(type=="u32") display_reg<unsigned long>(reg_number);
					else if(type=="i32") display_reg<signed long>(reg_number);
					else if(type=="u64") display_reg<unsigned long long>(reg_number);
					else if(type=="i64") display_reg<signed long long>(reg_number);
					else if(type=="f32") display_reg<float>(reg_number);
					else if(type=="f64") display_reg<double>(reg_number);
				}
				else{
					Parser parser(input);
					String type=parser.get_type();
					if(!parser.ok){
						cout<<"error parsing type"<<endl;
						continue;
					}
					Optional<size_t> array_size=parser.get_array_size();
					if(!parser.ok){
						cout<<"error parsing array size"<<endl;
						continue;
					}
					size_t address=parser.get_address();
					if(!parser.ok){
						cout<<"error parsing address"<<endl;
					}
		
					if(type=="char") display<char>(address,array_size);
					else if(type=="u8") display<unsigned char>(address,array_size);
					else if(type=="i8") display<signed char>(address,array_size);
					else if(type=="u16") display<unsigned short>(address,array_size);
					else if(type=="i16") display<signed short>(address,array_size);
					else if(type=="u32") display<unsigned long>(address,array_size);
					else if(type=="i32") display<signed long>(address,array_size);
					else if(type=="u64") display<unsigned long long>(address,array_size);
					else if(type=="i64") display<signed long long>(address,array_size);
					else if(type=="f32") display<float>(address,array_size);
					else if(type=="f64") display<double>(address,array_size);
					else if(type=="str") puts((char*)&memory[address]);
				}

			}	
		}
		struct Parser{
			const String& s;
			size_t i;
			bool ok;
			bool check(){
				if(i>=s.size()) ok=false;
				return ok;
			}
			Parser(const String& s):s(s),i(0),ok(true){}
			String get_type(){
				String type;
				while(s[i]!=' ' && s[i]!='['){
					if(!check()) break;
					type+=s[i++];
				}
				return type;
			}
			
			Optional<size_t> get_array_size(){
				if(s[i]=='['){ i++;
					String array_size;
					while(s[i]!=']'){
						if(!check() || !is_numeric(s[i])){
							ok=false;
							return {};
						}
						array_size+=s[i++];
					}
					i++;
					return array_size.parse<size_t>();
				}
				else return {};
			}
			size_t get_address(){
				String address;
				if(s[i]==' ')i++;
				while(s[i]){
					if(!is_numeric(s[i])){
						ok=false;
						return 0;
					}
					address+=s[i++];
				}
				return address.parse<size_t>();
			}
		};
	};
}
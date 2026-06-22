class Tokenizer{
	enum State:char{RESET,CODE,ASCII,STRING}state=RESET;
	const String& input;	// La entrada nunca tiene espacios al inicio ni al final
	Token current;
	size_t i;
	void reading_code(Vector<Token>& tokens){
		switch(input[i]){
			case ',':
			case '(':
			case ')':
			case '+':
			case '-':
			case '/':
			case '*':
			case '[':
			case ']':	tokens.push_back(static_cast<Token&&>(current));
						current.type=TokType::symbol;
						current+=input[i];
						tokens.push_back(static_cast<Token&&>(current));
						current.type=TokType::unknown;
						state=RESET;
						break;
			case ':':	current.type=TokType::label_decl;
						tokens.push_back(static_cast<Token&&>(current));
						current.type=TokType::unknown;
						state=RESET;
						break;
			case ' ': 	tokens.push_back(static_cast<Token&&>(current));
						state=RESET;
						break;
			case '"':	throw String("Expected space before the beginning of a string");break;
			case '\'':	throw String("Expected space before the beginning of an ASCII character");break;
			default:  	current+=input[i];break;
		}
	}
	void reading_ascii_char(Vector<Token>& tokens){
		switch(input[i]){
			case '\\':	if(++i<input.size()){
							switch(input[i]){
								case '0':	current+="0";break;
								case 'n':	current+="10";break;
								case 't': 	current+="9";break;
								case 'r':	current+="13";break;
								case '\\': 	current+="92";break;
								case '"': 	current+="34";break;
								case '\'': 	current+="39";break;
								default:	throw String("Unknown escape sequence '\\")+String(input[i])+String('\'');
							}
						}else throw "Stray '"_s;
						break;
			default: 	current+=to_str(input[i]);
		}
		i++;
		if(i<input.size() && input[i++]=='\''){
			if(i==input.size() || input[i]==' ') {
				tokens.push_back(static_cast<Token&&>(current));
				state=RESET;
			}
			else if(Vector<char>({',','(',')','[',']','+','-','/','*'}).contains(input[i])){
				tokens.push_back(static_cast<Token&&>(current));
				current.type=TokType::symbol;
				current+=input[i];
				tokens.push_back(static_cast<Token&&>(current));
				current.type=TokType::unknown;
				state=RESET;
			}
			else throw String("Expected space or symbol after end of ascii character");
		}
		else throw String("Expected ' to close char");
	}
	void reading_string(Vector<Token>& tokens){
		switch(input[i]){
			case '\\': 	if(++i<input.size()){
							switch(input[i]){
								case '0':	current+='\0';break;
								case 'n':	current+='\n';break;
								case 't': 	current+='\t';break;
								case 'r':	current+='\r';break;
								case '\\': 	current+='\\';break;
								case '"': 	current+='\"';break;
								case '\'': 	current+='\'';break;
								default:	throw "Unknown escape sequence '\\"_s+String(input[i])+'\'';
							}
						}else throw "Multiline strings not allowed"_s;
						if(i+1==input.size()) throw String("Unterminated string");
						break;
			case '"':	if(i+1!=input.size()) throw String("No code expected after closing a string");
						tokens.push_back(static_cast<Token&&>(current));
						current.type=TokType::unknown;
						state=RESET;
						break;
			default:  	current+=input[i];
						if(i+1==input.size()) throw String("Unterminated string");
						break;
		}
	}
	void reset(Vector<Token>& tokens){
		switch(input[i]){
			case ',':
			case '(':
			case ')':
			case '+':
			case '*':
			case '-':
			case '/':
			case '[':
			case ']':	current.type=TokType::symbol;
						current+=input[i];
						tokens.push_back(static_cast<Token&&>(current));
						current.type=TokType::unknown;
						state=RESET;
			case ' ':	break;
			case '"': 	state=STRING;
						current.type=TokType::string;break;
			case '\'':	state=ASCII;break;
			default: 	current+=input[i];state=CODE;break;
		}
	}
public:
	Tokenizer(const String& in):input(in){}
	Vector<Token> run(){
		Vector<Token> tokens;
		for(i=0;i<input.size();i++){
			switch(state){
				case RESET:	reset(tokens);break;
				case CODE: 	reading_code(tokens);break;
				case ASCII:	reading_ascii_char(tokens);break;
				case STRING:reading_string(tokens);break;
			}
		}
		if(state==CODE) tokens.push_back(static_cast<Token&&>(current));
		return tokens;
	}
};
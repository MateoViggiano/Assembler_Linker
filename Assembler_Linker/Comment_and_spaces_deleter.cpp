class Comment_and_spaces_deleter{
	using String=mpv::String;
	enum State:unsigned char{space,code,string1,string2,backslash,slash,multiline_comment,comment} state=State::space,prev_state;
	const String& input;
	size_t i,backslash_newline_count=0,last_multiline_comment_open;//newline_counter cuenta cuantas lineas se extendio un slash usando un backslash antes de pasar al estado comment o multiline_comment
	bool ready_to_end_multiline=false;
	public:
	String output;
	bool comment_error=false;
	Comment_and_spaces_deleter(const String& text):input(text){}
	//Comment_and_spaces_deleter(String&& text):input(static_cast<String&&>(text)){}
	const String& run(){
		for(i=0;i<input.size();i++){
			switch(state){
				case space:				reading_spaces();break;
				case code:				reading_code();break;
				case string1:			reading_string1();break;
				case string2:			reading_string2();break;
				case backslash:			reading_backslash();break;
				case slash:				reading_slash();break;
				case multiline_comment:	reading_multiline_comment();break;
				case comment:			reading_comment();break;
			}			
		}
		if(state==backslash){
			output+=String("\\\n")*backslash_newline_count+'\\';
		}
		else if(state==slash){
			output+=String("\\\n")*backslash_newline_count;
		}
		else if(state==multiline_comment){
			comment_error=true;
		}
		return output;
	}
	size_t get_error_line(){
		size_t line_counter=0;
		for(i=0;i<last_multiline_comment_open;i++){
			if(input[i]=='\n') line_counter++;
		}
		return line_counter+1;//+1 porque los editores de texto empiezan desde la linea 1;
	}
	void reading_spaces(){
		switch(input[i]){
			case '"':	output+='"';
						state=string1;break;
			case '\'':	output+='\'';
						state=string2;break;
			case '/':	output+='/';
						state=slash;
						backslash_newline_count=0;
						break;
			case '\n':	output+='\n';break;
			case ' ':
			case '\r':
			case '\t':	break;
			default:	output+=input[i];
						state=code;break;
		}
		prev_state=space;
	}
	void reading_code(){
		switch(input[i]){
			case '"':	output+='"';
						state=string1;break;
			case '\'':	output+='\'';
						state=string2;break;
			case '/':	output+='/';
						state=slash;
						backslash_newline_count=0;
						break;
			case '\n':	output+='\n';
						state=space;break;
			case ' ':
			case '\r':
			case '\t':	output+=' ';
						state=space;break;
			default: 	output+=input[i];break;
		}
		prev_state=code;
	}
	void reading_backslash(){
		if(prev_state==multiline_comment){
			if(input[i]=='\n')
				output+='\n';
			else ready_to_end_multiline=false;
			state=prev_state;
		}
		else if(prev_state==comment){
			if(input[i]=='\n')
				output+='\n';
			state=prev_state;
		}
		else if(prev_state==slash){
			if(input[i]=='\n'){
				backslash_newline_count++;
				state=prev_state;
			}
			else{//will probably lead to a compiling error
				output+=String("\\\n")*backslash_newline_count+'\\'+input[i];
				state=code;
			}
		}
		else if(prev_state==string1 || prev_state==string2){
			output+=input[i];
			state=prev_state;
		}
		prev_state=backslash;
	}
	void reading_slash(){
		switch(input[i]){
			case '\\':	state=backslash;break;
			case '/':	output.del_back();
						output+=String('\n')*backslash_newline_count;backslash_newline_count=0;
						state=comment;break;
			case '*':	last_multiline_comment_open=i-1-2*backslash_newline_count;
						output.del_back();
						output+=String('\n')*backslash_newline_count;backslash_newline_count=0;
						state=multiline_comment;break;
			case '"':	output+=String("\\\n")*backslash_newline_count+'"';backslash_newline_count=0;
						state=string1;break;
			case '\'':	output+=String("\\\n")*backslash_newline_count+'\'';backslash_newline_count=0;
						state=string2;break;
			case '\n':	output+=String("\\\n")*backslash_newline_count+'\n';backslash_newline_count=0;
						state=space;break;
			case ' ':
			case '\t':	output+=String("\\\n")*backslash_newline_count+' ';backslash_newline_count=0;
						state=space;break;
			default:	output+=String("\\\n")*backslash_newline_count+input[i];backslash_newline_count=0;
						state=code;break;
		}
		prev_state=slash;
	}
	void reading_comment(){
		switch(input[i]){
			case '\\':	state=backslash;break;
			case '\n':	output+='\n';
						state=space;break;
			default:	break;
		}
		prev_state=comment;
	}
	void reading_multiline_comment(){
		switch(input[i]){
			case '*':	ready_to_end_multiline=true;break;
			case '/':	if(ready_to_end_multiline==true){
							output+=' ';
							state=space;
						}break;
			case '\n':	output+='\n';
						ready_to_end_multiline=false;
						break;
			case '\\':	state=backslash;break;
			default:	ready_to_end_multiline=false;break;
		}
		prev_state=multiline_comment;
	}
	void reading_string1(){
		switch(input[i]){
			case '\\':	output+='\\';	//these handles the case where string is extended to the next line using \enter, or \" is found
						state=backslash;break;
			case '\n':	output+='\n';
						state=space;break;
			case '"':	output+='"';
						state=space;break;
			default:	output+=input[i];break;
		}
		prev_state=string1;
	}
	void reading_string2(){	
		switch(input[i]){
			case '\\':	output+='\\';	//these handles the case where string is extended to the next line using \enter, or \' is found
						state=backslash;break;
			case '\n':	output+='\n';
						state=space;break;
			case '\'':	output+='\'';
						state=space;break;
			default:	output+=input[i];break;
		}
		prev_state=string2;
	}
};
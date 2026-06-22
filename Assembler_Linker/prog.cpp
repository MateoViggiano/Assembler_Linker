#include<iostream>
#include<fstream>
#include"F:/C++/viggianolib/viggiano"
using namespace std;
using namespace mpv;
void printstring(const String& s){
	for(char c:s){
		if(c=='\n') cout<<"\\n\n";
		else if(c=='\r') cout<<"\\r";
		else cout<<c;
	}
}
int main(){
	String fname="main.asm",text;
	std::ifstream file(fname.c_str(),ios::binary);
	text.read(file);
	printstring(text);
}

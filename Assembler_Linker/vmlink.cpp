#include<iostream>
#include<fstream>
#include<stdint.h>
#include<viggiano>
using namespace mpv;
using std::cout;
using std::endl;
using std::cin;
struct RelativeAddress{
	uint64_t address;
	Vector<uint64_t> references;
    bool is_global;
    RelativeAddress()=default;
	RelativeAddress(uint64_t address,const Vector<uint64_t>& v):address(address),references(v),is_global(false){}
};
std::ostream& operator<<(std::ostream& out,const RelativeAddress& ra){
    out<<'&'<<ra.address<<' '<<ra.references;
    return out;
}
struct OF{
	Map<String,RelativeAddress> labels;
	Map<String,Vector<uint64_t>> extern_labels;
    Vector<uint8_t> binary;
    String fname;
    OF(const char* fname):fname(fname){
        std::ifstream file(fname,std::ios::in | std::ios::binary);
        if(!file.is_open()){
            std::cout<<fname<<" could not be opened\n";
            exit(99);
        }
        uint32_t table_size;
        file.read(reinterpret_cast<char*>(&table_size),sizeof(table_size));
        for(uint32_t i=0;i<table_size;i++){
            RelativeAddress ra;
            ra.is_global=file.get()=='G';
            String s;
            file>>s;
            file.read(reinterpret_cast<char*>(&ra.address),sizeof(ra.address));
            uint32_t lsize;
            file.read(reinterpret_cast<char*>(&lsize),sizeof(lsize));
            ra.references.resize(lsize);
            for(uint32_t j=0;j<lsize;j++){
                file.read(reinterpret_cast<char*>(ra.references.get_array()+j),sizeof(uint64_t));
            }
            labels.emplace(s,static_cast<RelativeAddress&&>(ra));
        }
        file.read(reinterpret_cast<char*>(&table_size),sizeof(table_size));
        for(uint32_t i=0;i<table_size;i++){
            String s;
            file>>s;
            uint32_t lsize;
            file.read(reinterpret_cast<char*>(&lsize),sizeof(lsize));
            Vector<uint64_t> vec(lsize);
            for(uint32_t j=0;j<lsize;j++){
                file.read(reinterpret_cast<char*>(vec.get_array()+j),sizeof(uint64_t));
            }
            extern_labels.emplace(s,static_cast<Vector<uint64_t>&&>(vec));
        }
        uint64_t binary_size;
        file.read(reinterpret_cast<char*>(&binary_size),sizeof(binary_size));
        binary.resize(binary_size);
        file.read((char*)binary.get_array(),binary_size);
        debug_print_OF();
    }
    void debug_print_OF(){
        puts("Labels:");
        for(auto& l:labels){
            cout<<l<<endl;
        }
        puts("Extern labels:");
        for(auto& l:extern_labels){
            cout<<l<<endl;
        }
        cout<<"\nBinary size: "<<binary.size()<<endl;
    }
    void replace_addresses(){
        for(const MapPair<String,RelativeAddress>& l:this->labels){
            for(uint64_t reference:l.val.references){
                *reinterpret_cast<uint64_t*>(this->binary.get_array()+reference)=l.val.address;
            }
        }
    }
};
bool solve_references(OF& current,Vector<OF>& object_files){
    bool error=false;
    for(MapPair<String,Vector<uint64_t>>& l:current.extern_labels){
        unsigned short found=0;
        for(OF& of:object_files){
            if(&current==&of) continue;
            // auto ra=of.labels.find(l.key);
            // if(ra!=of.labels.end() && ra->val.is_global){
            //     current.labels.emplace(l.key,RelativeAddress(ra->val.address,l.val));
            //     found++;
            // }
            Optional<RelativeAddress> ra=of.labels.get(l.key);
            if(ra.has_value() && ra.value().is_global){
                current.labels.emplace(l.key,RelativeAddress(ra.value().address,l.val));
                found++;
            }
        }
        switch(found){
            case 0: cout<<current.fname<<": Undefined reference to '"<<l.key<<"'\n";
                    error=true;
            case 1: break;
            default:cout<<current.fname<<": Found multiple references to "<<l.key<<"'\n";
                    error=true;
        }
    }
    current.extern_labels.clear();
    return error;
}
bool find_main(Vector<OF>& object_files,uint64_t* mainaddress){
    unsigned short found=0;
    for(OF& of:object_files){
        const String s("main");
        auto p=of.labels.find(s);
        if(p!=of.labels.end() && p->val.is_global){
            *mainaddress=p->val.address;
            found++;
        }
    }
    switch(found){
        case 0: cout<<"Undefined reference to 'main'\n";
                return true;
        case 1: return false;
        default:cout<<"Found multiple references to 'main'\n";
                return true;
    }
}
Vector<uint8_t> link(Vector<OF>& object_files){
    Vector<uint8_t> executable={0b01110000,0,0,0,0,0,0,0,0,0b11011101,0b00001010}; //call main , exit r10
    uint64_t current_position=executable.size();
    for(OF& of:object_files){
        of.labels.foreach([current_position](MapPair<String,RelativeAddress>& x){x.val.address+=current_position;});
        current_position+=of.binary.size();
    }
    unsigned int errors=find_main(object_files,reinterpret_cast<uint64_t*>(executable.get_array()+1));
    for(OF& of:object_files){
        errors+=solve_references(of,object_files);
    }
    if(errors>0){
        puts("Program terminated");
        exit(100);
    }
    for(OF& of:object_files){
        of.replace_addresses();
        executable+=of.binary;
    }
    return executable;
}
int main(int argc,const char** argv){
    Vector<OF> object_files;
    if(argc<3){
        cout<<"Expected more than 2 arguments\n";
        return 1;
    }
    for(int i=2;i<argc;i++){
        object_files.emplace_back(argv[i]);
    }
    Vector<uint8_t> executable=link(object_files);
    String fname=argv[1];
    fname+=".vmexe";
    std::ofstream output(fname.c_str(),std::ios::out | std::ios::binary);
    if(!output.is_open()){
        cout<<"Unable to create "<<fname<<endl;
        return 1;
    }
    output.write((char*)executable.get_array(),executable.size());
    cout<<"Executable size: "<<executable.size()<<endl;
}
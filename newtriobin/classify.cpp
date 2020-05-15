#include <iostream>
#include <map>
#include <set>
#include <fstream>
#include <cassert>
#include <ctime>
#include <thread>
#include <stack>
#include <mutex>
#include <vector>
#include <chrono>
#include <getopt.h>
#include "gzstream/gzstream.h"

void logtime(){
    time_t now = time(0);
    char* dt = ctime(&now);
    std::cerr<<dt<<std::endl;
}
//
// kmer relate functions
//
std::map<char,char> g_oppo ;
void InitMap(){
    g_oppo['a']='T';
    g_oppo['A']='T';
    g_oppo['g']='C';
    g_oppo['G']='C';
    g_oppo['c']='G';
    g_oppo['C']='G';
    g_oppo['t']='A';
    g_oppo['T']='A';
    g_oppo['n']='N';
    g_oppo['N']='N';
}
std::string reverse_complement(const std::string & kmer){
    std::string ret = kmer;
    for(int i = 0 ; i< (int)kmer.size(); i++)
        ret[kmer.size()-i-1] = g_oppo[kmer[i]];
    return ret ;
}

//
// load & cache maternal unique kmer & paternal unique kmer
//
#define HAPLOTYPES 2
std::set<std::string> g_kmers[HAPLOTYPES];
int g_K=0;
int total_kmers[HAPLOTYPES];
void load_kmers(const std::string & file,int index){
    std::ifstream ifs(file);
    std::string line;
    int total_kmer = 0 ;
    if(index==0){
        std::getline(ifs,line);
        g_K = line.size();
        g_kmers[index].insert(line);
        g_kmers[index].insert(reverse_complement(line));
        total_kmer++;
    }
    while(!std::getline(ifs,line).eof()){
        g_kmers[index].insert(line);
        g_kmers[index].insert(reverse_complement(line));
        total_kmer++;
    }
    total_kmers[index] = total_kmer ;
    std::cerr<<"Recorded "<<total_kmer<<" haplotype "<<index<<" specific "<<g_K<<"-mers\n"; 
}
//
// Output cache
//
struct OutputCache {
    struct ReadInfo {
        long long  read_id;
        std::string name;
        double hapCounts[2];
        std::string hapCounts_Str() const {
            std::string ret ;
            return ret ;
        }
    };
    std::mutex mm;
    std::map<int , ReadInfo> output_cache;
    void SaveOutput(const ReadInfo & data){
        mm.lock();
        output_cache[data.read_id] = data;
        mm.unlock();
    }
    void PrintOutput() const {
        for( const auto & pair : output_cache) {
            const auto & data = pair.second;
            double readHapCount = 0 ; 
            double secondBest = 0 ;
            std::string readHap = "haplotype";
            for( int i = 0 ; i<HAPLOTYPES ; i++ ) {
                if( data.hapCounts[i] > 0 and data.hapCounts[i] < readHapCount and data.hapCounts[i] > secondBest)
                    secondBest = data.hapCounts[i];

                if( data.hapCounts[i] > 0 and data.hapCounts[i] > readHapCount){
                    readHap +=std::to_string(i);
                    secondBest = readHapCount;
                    readHapCount = data.hapCounts[i];
                }

                if( secondBest == 0 and readHapCount != 0 )
                    printf("Read %s classified as %s with %s\n",data.name.c_str(), readHap.c_str(), data.hapCounts_Str().c_str());
                else if( readHapCount == 0 and secondBest == 0 )
                    printf("Read %s has no distringuising mers, ambiguous\n",(data.name.c_str()));
                else if( readHapCount == 0 and secondBest != 0 ) {
                    printf("Not possible!\n");
                    exit(1);
                }
                else if ( readHapCount / secondBest > 1 )
                    printf ("Read %s classified as %s due to %f but second best existed with %f all counts %s\n",data.name.c_str(), readHap.c_str(), readHapCount, secondBest, data.hapCounts_Str().c_str());
                else
                    printf("Read %s is amibigous unalbe to classify due to %f and %f\n",data.name.c_str(), readHapCount, secondBest); 
            }
        }
    }
};

//
//reads relate functions
//

struct ReadBasic{
    long long id ;
    std::string head ;
    std::string seq ;
};
struct MultiThread {
    int t_nums ;
    bool end;
    bool busy;
    OutputCache data;

    std::vector< std::stack<ReadBasic> >  caches;
    std::mutex * locks;
    std::thread ** threads; 

    MultiThread(int t_num){
        t_nums = t_num ;
        busy = false;
        end=false;

        locks = new std::mutex[t_num];
        threads = new std::thread*[t_num];
        for(int i = 0 ; i< t_num ; i++){
            caches.push_back(std::stack< ReadBasic >());
            threads[i] = new std::thread([this,i](){int index=i ;Worker(index); });
        }
    }
    void wait(){
        for(int i = 0 ; i <t_nums; i++){
            threads[i]->join();
            delete threads[i];
        }
    }
    ~MultiThread(){
        delete [] threads;
        delete [] locks;
    }

    void Worker(int index){
        ReadBasic job;
        while(true){
            locks[index].lock();
            if( caches[index].empty() ){
                busy = false ;
                locks[index].unlock();
                if(end) return ;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                continue;
            }
            if( ! caches[index].empty() ){
                std::swap(job,caches[index].top());
                if( caches[index].size() > 10000 ) busy = true ;
                else if ( caches[index].size() < 3000 ) busy = false ;
                caches[index].pop();
                locks[index].unlock();
                process_reads(job.head,job.seq,job.id);
            } else 
                locks[index].unlock();
        }
    }

    void process_reads(const std::string & head ,
            const std::string & seq , long long  readid) {
        OutputCache::ReadInfo tmp ;
        tmp.read_id = readid ;
        tmp.name = head ;
        for(int i = 0 ; i< HAPLOTYPES; i++ ) tmp.hapCounts[i]=0;
        for(int i = 0 ; i <(int)seq.size()-g_K+1;i++){
            std::string kmer = seq.substr(i,g_K);
            for( int j = 0 ; j< HAPLOTYPES; j++ )
                if( g_kmers[j].find(kmer) != g_kmers[j].end() )
                    tmp.hapCounts[j] ++ ;
        }
        for( int j = 0 ; j< HAPLOTYPES; j++ )
            tmp.hapCounts[j] /= total_kmers[j];
        data.SaveOutput(tmp);
    }
    void submit(std::string & head ,std::string & seq, long long id){
        while( busy ) { std::this_thread::sleep_for(std::chrono::seconds(1));}
        int tid = id % t_nums;
        ReadBasic tmp ;
        std::swap(tmp.head,head);
        std::swap(tmp.seq,seq);
        tmp.id = id ;
        locks[tid].lock();
        caches[tid].push(std::move(tmp));
        locks[tid].unlock();
    }
};

std::istream * openfile( const std::string & file ){
    std::istream *in ;
    bool gz_file = false;
    if( file.size() > 3 ) {
        int end=file.size() ;
        if( file[end-3] == '.' && file[end-2] == 'g' && file[end-1]=='z' ) {
            gz_file = true ;
        }
    }
    if ( gz_file )
        in = new igzstream(file.c_str());
    else 
        in = new std::ifstream(file);
    return in ;
}

void processFastq(const std::string & file,int t_num){
    std::string head;
    std::string seq;
    std::string tmp;
    MultiThread mt(t_num);
    std::istream *in = openfile(file);
    long long id = 0 ;
    while(!std::getline(*in,head).eof()){
        std::getline(*in,seq);
        mt.submit(head,seq,id );
        std::getline(*in,tmp);
        std::getline(*in,tmp);
        id ++ ;
    }
    mt.end=true;
    delete in ;
    mt.wait();
    mt.data.PrintOutput();
}

void printUsage(){
    std::cerr<<"Uasge :\n\tclassify_read --hap hap0.kmer --hap hap1.kmer --read read1.fq [--read read2.fq] [--thread t_num (8 default) ] "<<std::endl;
    std::cerr<<"notice : --read accept file in gzip format , but file must end by \".gz\""<<std::endl;
}
//
// Main function
//
int main(int argc ,char ** argv ) {
    InitMap();
    static struct option long_options[] = {
        {"hap",   required_argument, NULL, 'p'},
        {"read",  required_argument, NULL, 'r'},
        {"thread",required_argument, NULL, 't'},
        {"help",  no_argument,       NULL, 'h'},
        {0, 0, 0, 0}
    };
    static char optstring[] = "p:r:t:h";
    std::vector<std::string> haps;
    std::vector<std::string> read;
    int t_num=8;
    while(1){
        int c = getopt_long(argc, argv, optstring, long_options, NULL);
        if (c<0) break;
        switch (c){
            case 'p':
                haps.push_back(std::string(optarg));
                break;
            case 'r':
                read.push_back(std::string(optarg));
                break;
            case 't':
                t_num = atoi(optarg);
                break;
            case 'h':
            default :
                printUsage();
                return -1;
        }
    }
    if( haps.size() != HAPLOTYPES || read.empty()|| t_num< 1) {
        printUsage();
        return -1;
    }
    logtime();
    std::cerr<<"__START__"<<std::endl;
    for( int i = 0 ; i<HAPLOTYPES ; i ++ ) {
        std::cerr<<"__load hap"<<i<<" kmers__"<<std::endl;
        logtime();
        load_kmers(haps[i],i);
    }
    for(const auto r : read ){
        std::cerr<<"__process read: "<<r<<std::endl;
        logtime();
        processFastq(r,t_num);
        std::cerr<<"__process read done__"<<std::endl;
        logtime();
    }
    std::cerr<<"__END__"<<std::endl;
    logtime();
}
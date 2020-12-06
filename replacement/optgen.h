// Based on optgen code obtained from Cache Replacement Championship(crc2)
// Website --> https://crc2.ece.tamu.edu/?page_id=53
using namespace std;
// Size of each liveliness vector defined below. This tracks the 8x history of 16 way cache, Hawkeye paper says to quantize the accesses by 4. in which case, this should be lesser (1/4th of 128) ??. Leaving it here like this, for now
#define OPTGEN_VECTOR_SIZE 128
// Optgen uses Sampler Cache to track all 8x previous access time stamp, the dimensions of the cache are defined below
// 2800 number derived from Hawkeye paper, can be changed in future
// ways also chossen randomly, doesnt matter much i think
#define SAMPLER_CACHE_SIZE 2800
#define SAMPLER_WAYS 8
#define SAMPLER_SETS SAMPLER_CACHE_SIZE/SAMPLER_WAYS
#define SAMPLER_SET_MASK  511
#define TIMER_SIZE 1024
#include <iostream>
#include <math.h>
#include <set>
#include <vector>
// Do CRC for tag caluclation
uint64_t CRC( uint64_t _blockAddress){
    static const unsigned long long crcPolynomial = 3988292384ULL;
    unsigned long long _returnVal = _blockAddress;
    for( unsigned int i = 0; i < 32; i++ )
        _returnVal = ( ( _returnVal & 1 ) == 1 ) ? ( ( _returnVal >> 1 ) ^ crcPolynomial ) : ( _returnVal >> 1 );
    return _returnVal;
}
// information tracked for each address in the sampled set
// last quanta needed to upate the liveliness interval
// We need not track the Previous-PC for glider, unlike Hawkeye, we wont be updating weights corresponding to previous PC
// Instead we will be updating the table values for current PC and PCHR
struct ADDR_INFO{
    uint64_t addr;
    uint32_t last_quanta;
    uint64_t PC;
    uint32_t lru;
    // function to initialize the state of each ADDR_INFO
    void init(unsigned int curr_quanta){
        last_quanta = 0;
        PC = 0;
        lru = 0;
    }
    // function to update the state of ADDR_INFO on access to given addr
    void update(unsigned int curr_quanta, uint64_t _pc, bool prediction){
        last_quanta = curr_quanta;
        PC = _pc;
    }
};
class sampler_cache{
    private:
    // sampler cache implemented as vector of maps 
    vector<map<uint64_t, ADDR_INFO> > addr_history;
    public:
    // constructor for sampler_cache which initializes the sampler cache
    sampler_cache(){
        //initialize the sampler cache
        addr_history.resize(SAMPLER_SETS);
        // Now initialize all entries in map 
        for(int i=0;i<SAMPLER_WAYS;i++){
            addr_history[i].clear();
        }
    }
    // destructor for sampler_cache 
    ~sampler_cache(){
        // Now clear each maps first
        for(int i=0;i<SAMPLER_WAYS;i++){
            addr_history[i].clear();
        }
        // Then clean the vector
        addr_history.clear();
    }
    // returns the hash into the sampling cache, which is 
    uint32_t hash_sample_set(uint64_t addr){
        return (uint32_t) ((addr >> 6) % SAMPLER_SETS);
    }
    // procedure for replacing an element in a given set of sampling set 
    void replace_addr_history_element(unsigned int sampler_set){
        // initialize the lru_address to 0
        uint64_t lru_addr = 0;
        // iterate through the given map at poistion using iterators
        for(map<uint64_t, ADDR_INFO>::iterator it=addr_history[sampler_set].begin(); it != addr_history[sampler_set].end(); it++){
            if((it->second).lru == (SAMPLER_WAYS-1)){
                //lru_time =  (it->second).last_quanta;
                lru_addr = it->first;
                break;
            }
        }
        addr_history[sampler_set].erase(lru_addr);
    }
    // procedure for updating addr_history 
    void update_addr_history_lru(unsigned int sampler_set, unsigned int curr_lru){
        for(map<uint64_t, ADDR_INFO>::iterator it=addr_history[sampler_set].begin(); it != addr_history[sampler_set].end(); it++){
            if((it->second).lru < curr_lru){
                (it->second).lru++;
                assert((it->second).lru < SAMPLER_WAYS); 
            }
        }
    }
    // procedure for probing into addr_history, returns true if present, false if its not 
    bool probe_sampler_cache(uint32_t sampler_set, uint64_t sampler_tag){
        if((addr_history[sampler_set].find(sampler_tag) != addr_history[sampler_set].end())){
            return true;
        }
        else if(addr_history[sampler_set].find(sampler_tag) == addr_history[sampler_set].end()){
            return false;
        }
        return false;
    }
    // Function to add element into SamplerCache
    bool add_element(uint32_t sampler_set, uint64_t sampler_tag,uint64_t curr_quanta){
        // First check if the number of ways are full
        if(addr_history[sampler_set].size() == SAMPLER_WAYS){
            #ifdef DEBUG_PRINT
            printf("sampler_set: %d was found to be full, will be replacing one", sampler_set);
            #endif
            replace_addr_history_element(sampler_set);
        }
        // By this point we should have removed atleast one element from the set
        assert(addr_history[sampler_set].size() < SAMPLER_WAYS);
        // Now initialize the block we want, not sure if this works or not, coz there is no mapping entry for sampler_tag
        addr_history[sampler_set][sampler_tag].init(curr_quanta);
        // Update the LRU as well, as this is placed just now
        update_addr_history_lru(sampler_set, SAMPLER_WAYS-1);
        return true;
    }
    // Function to get the last quanta, returns it in the form of uint64_t 
    uint64_t get_last_quanta(uint32_t sampler_set, uint64_t sampler_tag){
        assert((addr_history[sampler_set].find(sampler_tag) != addr_history[sampler_set].end()));
        map<uint64_t, ADDR_INFO>::iterator it = addr_history[sampler_set].find(sampler_tag);
        return it->second.last_quanta;
    }
    // Function to get the last quanta, returns it in the form of uint64_t 
    uint32_t get_lru(uint32_t sampler_set, uint64_t sampler_tag){
        assert((addr_history[sampler_set].find(sampler_tag) != addr_history[sampler_set].end()));
        map<uint64_t, ADDR_INFO>::iterator it = addr_history[sampler_set].find(sampler_tag);
        return it->second.lru;
    }
};
typedef struct OPTgen_entry{
    // Tracks the liveness_history per set
    vector<unsigned int> liveness_history;
    uint64_t CACHE_SIZE;
    // Numbers to tracks the decision statistics
    uint64_t num_cache;
    uint64_t num_dont_cache;
    uint64_t access;
    // function to initialize each OPTgen_entry
    void init(uint64_t size){
        num_cache = 0;
        num_dont_cache = 0;
        access = 0;
        CACHE_SIZE = size;
        liveness_history.resize(OPTGEN_VECTOR_SIZE, 0);
    }

    void add_access(uint64_t curr_quanta){
        access++;
        liveness_history[curr_quanta] = 0;
    }
    /*
    void add_prefetch(uint64_t curr_quanta){
        liveness_history[curr_quanta] = 0;
    }
    */
    bool should_cache(uint64_t curr_quanta, uint64_t last_quanta)
    {
        bool is_cache = true;
        unsigned int i = last_quanta;
        while (i != curr_quanta){
            if(liveness_history[i] >= CACHE_SIZE){
                is_cache = false;
                break;
            }

            i = (i+1) % liveness_history.size();
        }


        //if ((is_cache) && (last_quanta != curr_quanta))
        if ((is_cache)){
            i = last_quanta;
            while (i != curr_quanta){
                liveness_history[i]++;
                i = (i+1) % liveness_history.size();
            }
            assert(i == curr_quanta);
        }

        if (is_cache) num_cache++;
        else num_dont_cache++;

        return is_cache;    
    }

    uint64_t get_num_opt_hits(){
        return num_cache;

        uint64_t num_opt_misses = access - num_cache;
        return num_opt_misses;
    }
}OPTgen_entry;
class OPTgen{
    private:
    // data structure that has perset OPTgen structures
    // We use only 16 of these declared static arrays, the hawkeye implementation uses in such a way
    OPTgen_entry perset_optgen[LLC_SET];
    uint64_t perset_mytimer[LLC_SET];
    sampler_cache* SamplerCache;
    public:
    // In the constructor, we construct the sampler_cache
    OPTgen(){
        SamplerCache = new sampler_cache;
        for(int i=0;i<LLC_SET;i++){
            perset_mytimer[i] = 0;
            perset_optgen[i].init(LLC_WAY-2); // May be this should be LLC_WAYS, not sure why we made it LLC_WAYS-2
        }
    }
    // In the destructor we destrut the sampler_cache 
    ~OPTgen(){
        delete SamplerCache;
    }
    // This function only checks if the sampling set that has entery in Sampling cache, if it is, it retruns yes, else false
    bool CheckOPTgen(uint64_t PC,uint64_t paddr,uint32_t set){
        uint32_t sampler_set = SamplerCache->hash_sample_set(paddr);
        uint64_t sampler_tag = CRC(paddr >> 12) % 256;
        #ifdef DEBUG_PRINT
        cout << "PC: " << hex << PC <<" , PADDR: " << hex << paddr << " , sampler_set: " << sampler_set << ", sampler_tag: " << sampler_tag << endl;
        #endif
        return SamplerCache->probe_sampler_cache(sampler_set,sampler_tag);
    }
    // This function is only called after knowing that given set has an entry in the Sampling Cache 
    bool GetOPTgenDecision(uint64_t PC,uint64_t paddr,uint32_t set){
        //The current timestep 
        uint64_t curr_quanta = perset_mytimer[set] % OPTGEN_VECTOR_SIZE;
        unsigned int curr_timer = perset_mytimer[set];
        uint32_t sampler_set = SamplerCache->hash_sample_set(paddr);
        uint64_t sampler_tag = CRC(paddr >> 12) % 256;
        uint64_t last_quanta = SamplerCache->get_last_quanta(sampler_set,sampler_tag);
        if(curr_timer < last_quanta)
            curr_timer = curr_timer + TIMER_SIZE;
        bool wrap =  ((curr_timer -last_quanta) > OPTGEN_VECTOR_SIZE);
        last_quanta = last_quanta % OPTGEN_VECTOR_SIZE;
        if( !wrap && perset_optgen[set].should_cache(curr_quanta, last_quanta)){
            // Positively train the predictor
            return true;
        }
        else{
            // Negatively train the predictor
            return false;
        }
    }
    // this function is called when the CheckOptgen returns false, which means this element missed in SamplerCache
    bool AddOptgen(uint64_t PC,uint64_t paddr,uint32_t set){
        uint32_t sampler_set = SamplerCache->hash_sample_set(paddr);
        uint64_t sampler_tag = CRC(paddr >> 12) % 256;
        //The current timestep 
        uint64_t curr_quanta = perset_mytimer[set] % OPTGEN_VECTOR_SIZE;
        assert(SamplerCache->add_element( sampler_set,  sampler_tag, curr_quanta));
        perset_optgen[set].add_access(curr_quanta);
        SamplerCache->update_addr_history_lru(sampler_set,SAMPLER_WAYS-1);
        return true;
    }
    // Function that takes care of some book-keeping that neeeds to be done after obtaining the prediction
    bool CompleteOptgen(uint64_t PC,uint64_t paddr,uint32_t set){
        int32_t sampler_set = SamplerCache->hash_sample_set(paddr);
        uint64_t sampler_tag = CRC(paddr >> 12) % 256;
        //Increment the set timer
        perset_mytimer[set] = (perset_mytimer[set]+1) % TIMER_SIZE;
        //addr_history[sampler_set][sampler_tag].update(perset_mytimer[set], PC, new_prediction);
        //addr_history[sampler_set][sampler_tag].lru = 0;
        return true;
    }
    
};
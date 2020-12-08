using namespace std;
#include <iostream>
#include <math.h>
#include <set>
#include <vector>
#include <map>
#include <array>
#define NUM_PC_PCHR 5
#define NUM_WEIGHTS 16
#define NUM_ISVM 2048
#define DEBUG_PRINT
// Each entry in PCHR is modelled by the following struct
typedef struct pchr_entry{
  uint64_t PC;
  unsigned int LRU_count;
  bool valid;
}pchr_entry;
// Modelling PCHR as 5 member(PC) cache by the following class
class pchr{
  private:
  array <pchr_entry,NUM_PC_PCHR> pc_cache;
  public:
  // function to probe PCHR, returns true if the checking_PC is in PCHR, else it will return false
  bool probe(uint64_t checking_PC){
    // if the checking_PC is present return true
    for(int i=0;i<NUM_PC_PCHR;i++){
      if((pc_cache.at(i).PC == checking_PC) && (pc_cache.at(i).valid == true)){
        return true;
      }
    }
    // else return false
    return false;
  }
  // function called if the PC is indeed present in PCHR, just updates the LRU counter, to complete the access
  bool complete_access(uint64_t access_PC){
    bool return_value = false;
    // Access the access_PC, which means that we have to update the LRU counter here
    for(int i=0;i<NUM_PC_PCHR;i++){
      if((pc_cache.at(i).PC == access_PC) && (pc_cache.at(i).valid == true)){
        // Should make the LRU value 4, because we accessed it 
        pc_cache.at(i).LRU_count = NUM_PC_PCHR-1;
        return_value = true;
      }
      else if(pc_cache.at(i).valid == true) {
        // decrement the LRU value, with a floor of zero
        pc_cache.at(i).LRU_count = (pc_cache.at(i).LRU_count>0)?(pc_cache.at(i).LRU_count-1):0;
      }
    }
    #ifdef DEBUG_PRINT
    for(int i=0;i<NUM_PC_PCHR;i++){
      printf("LRU count at position %d is %d\n",i, pc_cache.at(i).LRU_count);
    }
    #endif
    return return_value;
  }
  bool insert(uint64_t inserting_PC){
    bool return_value = false;
    int insert_position = -1;
    // find a insert position in pchr, the position can be invalid or with LRU_count zero
    for(unsigned int i=0;i<NUM_PC_PCHR;i++){
      if(pc_cache.at(i).valid == false){
        // found a invalid element, hence we can exit from here
        insert_position = i;
        #ifdef DEBUG_PRINT
        printf("Inserting the PC: %lx, at the position: %d\n", inserting_PC, i);
        #endif
        break;
      }
      else if(pc_cache.at(i).LRU_count == 0){
        insert_position = i;
        #ifdef DEBUG_PRINT
        printf("Inserting the PC: %lx at the position: %d ", inserting_PC, i );
        #endif
        break;
      }
    }
    // By the point we are here, we should have an insert position, otherwise something is messed up
    assert((insert_position >= 0) && (insert_position < NUM_PC_PCHR));
    // Do the insertion here along with updating the LRU counts of remaining members
    for (int i=0;i<5;i++){
      if(i==insert_position){
        pc_cache.at(i).PC = inserting_PC;
        pc_cache.at(i).valid = true;
        pc_cache.at(i).LRU_count = NUM_PC_PCHR-1;
        return_value = true;
      }
      else if(pc_cache.at(i).valid== true){
        // decrement the LRU value, with a floor of zero
        pc_cache.at(i).LRU_count = (pc_cache.at(i).LRU_count>0)?(pc_cache.at(i).LRU_count-1):0;
      }
    }
    #ifdef DEBUG_PRINT
    for(int i=0;i<NUM_PC_PCHR;i++){
      printf("LRU count at position %d is %d\n",i,pc_cache.at(i).LRU_count);
    }
    #endif
    return return_value;
  }
  uint64_t access_PC_at(int index){
    if(pc_cache.at(index).valid == true){
      return pc_cache.at(index).PC;
    }
    else{
      return 0;
    }
  }
  pchr(void){
    // initialize the pchr here as this is the consturctor
    for(int i=0;i<NUM_PC_PCHR;i++){
      pc_cache.at(i).PC = 0;
      pc_cache.at(i).LRU_count = 0;
      pc_cache .at(i).valid = false;
    }
  }
};
typedef struct isvm_entry{
  map<unsigned int, int> weights;
}isvm_entry;
class glider_predictor{
  private:
  // declare a map that implements the ISVM table, map of a char key and isvm_entry(which is a map of 16 counters)
  map <unsigned int,isvm_entry> isvm_table;
  pchr* PCHR;
  // The paper uses a threshold of 60, this can be changed later based on result, or can be dynamically choosen
   int weights_sum_threshold = 60;
  // The paper uses a traning threshold, to stop incrementing the weights if their sum is greater than certai value
   int weights_training_threshold = 100;
  // hash function implementation
  unsigned int pchr_entry_hash(uint64_t PCHR_PC){
  // shift right by whatever the instruction word is and use the 4 bits preceeding that as a hash
  return (unsigned int)((PCHR_PC >> 2)&(NUM_WEIGHTS-1));
  }
  unsigned int ISVM_table_hash(uint64_t access_PC){
  // shift right by whatever the stride of instruction word is and use the 5 bits preceeding that as a hash
  return (unsigned int)((access_PC >> 2)&(NUM_ISVM-1));
  }
  public:
  glider_predictor(void){
    // First initialize the table of table which is isvm tables, whose each is a 16 weights
    isvm_entry empty_entry;
    for(int j=0;j<NUM_WEIGHTS;j++){
      empty_entry.weights.insert(pair<unsigned int,unsigned int>(j,0));
    }
    for(int i=0;i<NUM_ISVM;i++){
      isvm_table.insert(pair<unsigned int,isvm_entry>(i,empty_entry));
    }
    // Construct the PCHR object here
    PCHR = new pchr;
  }
  ~glider_predictor(void){
    // delete the PCHR object thats being used for glider
    delete PCHR;
  }
  bool get_prediction(uint64_t access_PC){
    // first get the hash of the access_PC to index into ISVM table
    unsigned int isvm_table_hash = ISVM_table_hash(access_PC);
    #ifdef DEBUG_PRINT
      printf("The hash for accessing PC into ISVM table is  %d\n", isvm_table_hash);
    #endif
    // get the iterator by hashing into the map function
    map<unsigned int,isvm_entry>::iterator isvm_iterator = isvm_table.find(isvm_table_hash);
    // This iterator shouldnt be the end iterator, asserting it here
    assert(isvm_iterator!=isvm_table.end());
    // Get the isvm from the iterator 
    isvm_entry access_PC_isvm = isvm_iterator->second;
    // Now access the 5 PC's in PCHR and generate the hashes
    unsigned int PC_in_PCHR;
    int weights_sum = 0;
    unsigned int PCHR_PC_hash = 0;
    for(int i =0;i<NUM_PC_PCHR;i++){
      if(PCHR->access_PC_at(i) != 0){
        // PCHR is not constructed by this point, probably asking the predictor to insert is the best thing to do ...
        // get the PC from PCHR
        PC_in_PCHR = PCHR->access_PC_at(i);
        // use the PC to generate the hash into the isvm
        PCHR_PC_hash = pchr_entry_hash(PC_in_PCHR);
        #ifdef DEBUG_PRINT
        printf("A PC in PCHR resulted in hash of %d\n",PCHR_PC_hash);
        #endif
        // use the hash to get the weight
        map<unsigned int,  int>::iterator  weight_iterator = access_PC_isvm.weights.find(PCHR_PC_hash);
        // Make sure the iterator is valid, by checking it against the end of the weight map
        assert(weight_iterator != access_PC_isvm.weights.end());
        // Accumulate the weights in weights_sum
        weights_sum = (weight_iterator->second)+ weights_sum;
      }
    }
    #ifdef DEBUG_PRINT
    printf("Printing the weights associated with ISVM:%lx, Table:%d\n", access_PC, isvm_table_hash);
    for(map<unsigned int, int>::iterator weight_iterator=access_PC_isvm.weights.begin();weight_iterator!=access_PC_isvm.weights.end();weight_iterator++){
      printf("%d,",weight_iterator->second);
    }
    printf("\n");
    printf("The sum of weights obtained by the PC's from PCHR hashing into the ISVM is %d\n",weights_sum);
    #endif
    // If the program has reached this point this means that we have already collected all the weights from PC's in PCHR
    if(weights_sum > weights_sum_threshold){
      return true;
    }
    else{ 
      return false;
    }
  }
  bool positive_train_predictor(uint64_t access_PC){
    // To train the predictor, first we need to hash in the access PC to get a ISVM
    unsigned int isvm_table_hash = ISVM_table_hash(access_PC);
    #ifdef DEBUG_PRINT
    printf("The hash for accessing PC into ISVM table is %d\n", isvm_table_hash );
    #endif
    // get the iterator by hashing into the map function
    map<unsigned int,isvm_entry>::iterator isvm_iterator = isvm_table.find(isvm_table_hash);
    // This iterator shouldnt be the end iterator, asserting it here
    assert(isvm_iterator!=isvm_table.end());
    // Get the isvm from the iterator 
    isvm_entry access_PC_isvm = isvm_iterator->second;
    // Now access the 5 PC's in PCHR and generate the hashes
    unsigned int PC_in_PCHR;
    int weights_sum = 0;
    unsigned int PCHR_PC_hash = 0;
    // Then obtain the hash from each PC in PCHR in to isvm to obtain weights
    for(int i =0;i<NUM_PC_PCHR;i++){
      PC_in_PCHR = PCHR->access_PC_at(i);
      if(PCHR->access_PC_at(i) != 0){
        // use the PC to generate the hash into the isvm
        PCHR_PC_hash = pchr_entry_hash(PC_in_PCHR);
        #ifdef DEBUG_PRINT
        printf("A PC in PCHR resulted in hash of %d\n",PCHR_PC_hash);
        #endif
        // use the hash to get the weight
        map<unsigned int,  int>::iterator  weight_iterator = access_PC_isvm.weights.find(PCHR_PC_hash);
        // Make sure the iterator is valid, by checking it against the end of the weight map
        assert(weight_iterator != access_PC_isvm.weights.end());
        // Accumulate the weights in weights_sum
        weights_sum += weight_iterator->second;
      }
    }  
    // Increment the weights if their threshold doesnt exceed 100
    if(weights_sum < 100){
      for(int i=0;i<NUM_PC_PCHR;i++){
        // get the PC from PCHR
        PC_in_PCHR = PCHR->access_PC_at(i);
        if(PCHR->access_PC_at(i) != 0){
          // use the PC to generate the hash into the isvm
          PCHR_PC_hash = pchr_entry_hash(PC_in_PCHR);
          #ifdef DEBUG_PRINT
          printf("A PC in PCHR resulted in hash of %d\n", PCHR_PC_hash);
          #endif
          // use the hash to get the weight
          map<unsigned int,  int>::iterator  weight_iterator = access_PC_isvm.weights.find(PCHR_PC_hash);
          // Make sure the iterator is valid, by checking it against the end of the weight map
          assert(weight_iterator != access_PC_isvm.weights.end());
          // Increment the each weight by 1
          weight_iterator->second += 1;
        }
      }
      #ifdef DEBUG_PRINT
      printf("Printing the weights associated with ISVM:%lx, Table:%d\n", access_PC, isvm_table_hash);
      for(map<unsigned int,  int>::iterator weight_iterator=access_PC_isvm.weights.begin();weight_iterator!=access_PC_isvm.weights.end();weight_iterator++){
        printf("%d,",weight_iterator->second);
      }
      printf("\n");
      #endif
      isvm_iterator->second = access_PC_isvm;
    }
    else{
      #ifdef DEBUG_PRINT
      printf("The sum of weights is more than training threshold, so we have not increment the weights\n");
      #endif
    }
    return true; 
  }
  bool negative_train_predictor(uint64_t access_PC){
    //To train the predictor, first we need to hash in the access PC to get a ISVM
    unsigned int isvm_table_hash = ISVM_table_hash(access_PC);
    #ifdef DEBUG_PRINT
      printf("The hash for accessing PC into ISVM table is %d\n", isvm_table_hash);
    #endif
    // get the iterator by hashing into the map function
    map<unsigned int,isvm_entry>::iterator isvm_iterator = isvm_table.find(isvm_table_hash);
    // This iterator shouldnt be the end iterator, asserting it here
    assert(isvm_iterator!=isvm_table.end());
    // Get the isvm from the iterator 
    isvm_entry access_PC_isvm = isvm_iterator->second;
    // Now access the 5 PC's in PCHR and generate the hashes
    unsigned int PC_in_PCHR;
    //unsigned int weights_sum = 0;
    unsigned int PCHR_PC_hash = 0;
    // Then obtain the hash from each PC in PCHR in to isvm to obtain weights
    for(int i=0;i<NUM_PC_PCHR;i++){
      // get the PC from PCHR
      PC_in_PCHR = PCHR->access_PC_at(i);
      if(PCHR->access_PC_at(i) != 0){
        // use the PC to generate the hash into the isvm
        PCHR_PC_hash = pchr_entry_hash(PC_in_PCHR);
        #ifdef DEBUG_PRINT
        printf("A PC in PCHR resulted in hash of %d\n", PCHR_PC_hash);
        #endif
        // use the hash to get the weight
        map<unsigned int,  int>::iterator  weight_iterator = access_PC_isvm.weights.find(PCHR_PC_hash);
        // Make sure the iterator is valid, by checking it against the end of the weight map
        assert(weight_iterator != access_PC_isvm.weights.end());
        // Decrement the each weight by 1
        weight_iterator->second = ((weight_iterator->second-1)>-7)?(weight_iterator->second-1):-7;
      }
    }
    isvm_iterator->second = access_PC_isvm;
    return true; 
  }
  bool complete_access(uint64_t access_PC){
    // This function is to complete the access of a access PC, after getting the prediction and traning
    // Probe if the PCHR already contains the access PC
    if(PCHR->probe(access_PC) == true){
      #ifdef DEBUG_PRINT
      printf("PCHR already contains the PC: %lx, hence we have to just update the LRU counters\n");
      #endif
      assert(PCHR->complete_access(access_PC));
    }
    else{
      // this means that access_PC is not in PCHR, we have to insert it 
      #ifdef DEBUG_PRINT
      printf("PCHR doesnot contain the PC: %lx, hence we have to insert into PCHR\n");
      #endif
      assert(PCHR->insert(access_PC));
    }
    return true;
  }
  uint64_t get_weight_sum(uint64_t access_PC){
    // This function is to get weight totals for obtaining confidence on how to place
    // first get the hash of the access_PC to index into ISVM table
    unsigned int isvm_table_hash = ISVM_table_hash(access_PC);
    #ifdef DEBUG_PRINT
      printf("The hash for accessing PC into ISVM table is  %d\n", isvm_table_hash);
    #endif
    // get the iterator by hashing into the map function
    map<unsigned int,isvm_entry>::iterator isvm_iterator = isvm_table.find(isvm_table_hash);
    // This iterator shouldnt be the end iterator, asserting it here
    assert(isvm_iterator!=isvm_table.end());
    // Get the isvm from the iterator 
    isvm_entry access_PC_isvm = isvm_iterator->second;
    // Now access the 5 PC's in PCHR and generate the hashes
    unsigned int PC_in_PCHR;
    int weights_sum = 0;
    unsigned int PCHR_PC_hash = 0;
    for(int i =0;i<NUM_PC_PCHR;i++){
      // get the PC from PCHR
      PC_in_PCHR = PCHR->access_PC_at(i);
      if(PCHR->access_PC_at(i) != 0){
        // use the PC to generate the hash into the isvm
        PCHR_PC_hash = pchr_entry_hash(PC_in_PCHR);
        #ifdef DEBUG_PRINT
        printf("A PC in PCHR resulted in hash of %d\n",PCHR_PC_hash);
        #endif
        // use the hash to get the weight
        map<unsigned int,  int>::iterator  weight_iterator = access_PC_isvm.weights.find(PCHR_PC_hash);
        // Make sure the iterator is valid, by checking it against the end of the weight map
        assert(weight_iterator != access_PC_isvm.weights.end());
        // Accumulate the weights in weights_sum
        weights_sum = (weight_iterator->second)+ weights_sum;
      }
    }
    return weights_sum;
  }
};
/** \file ConnectionManager.cc
 *  \todo clean Clean code when it is simple.
 *  \todo doc Write doxygen documentation.
 *  \authors cldurand,takaakimatsuo
 *
 *  \brief ConnectionManager
 */
#include <vector>
#include <omnetpp.h>
#include "RoutingDaemon.h"
#include "HardwareMonitor.h"
#include <rules/RuleSet.h>
#include <classical_messages_m.h>

using namespace omnetpp;
using namespace quisp::messages;
using namespace quisp::rules;

namespace quisp {
namespace modules {


  typedef struct swapping_rule_table{
    int left_partner;
    QNIC_type lqnic_type;
    int lqnic_index;
    int lres;
    int right_partner;
    QNIC_type rqnic_type;
    int rqnic_index;
    int rres;
  }swap_table;

/** \class ConnectionManager ConnectionManager.cc
 *  \todo Documentation of the class header.
 *
 *f  \brief ConnectionManager
 */
class ConnectionManager : public cSimpleModule
{
    private:
        int myAddress;
        RoutingDaemon *routingdaemon;
        HardwareMonitor *hardwaremonitor;

    protected:
        virtual void initialize() override;

        virtual void handleMessage(cMessage *msg) override;
        virtual void responder_alloc_req_handler(ConnectionSetupRequest *pk);
        virtual void intermediate_alloc_req_handler(ConnectionSetupRequest *pk);

        virtual void initiator_alloc_res_handler(ConnectionSetupResponse *pk);
        virtual void intermediate_alloc_res_handler(ConnectionSetupResponse *pk);
      
        virtual void responder_reject_req_handler(RejectConnectionSetupRequest *pk);
        virtual void intermediate_reject_req_handler(RejectConnectionSetupRequest *pk);

        // virtual RuleSet **generate_RuleSet(int *, int*, int*);
        virtual RuleSet* generateRuleSet_EntanglementSwapping(unsigned int RuleSet_id,int owner, int left_node, QNIC_type lqnic_type, int lqnic_index, int lres, int right_node, QNIC_type rqnic_type, int rqnic_index, int rres);
        virtual swap_table EntanglementSwappingConfig(int swapper_address, std::vector<int> path, QNIC_id_pair qnic_pair_info, int num_resources);
        virtual unsigned long createUniqueId();
};

Define_Module(ConnectionManager);

void ConnectionManager::initialize()
{
  EV<<"ConnectionManager booted\n";
  cModule *rd = getParentModule()->getSubmodule("rd");
  routingdaemon = check_and_cast<RoutingDaemon *>(rd);
  cModule *hm = getParentModule()->getSubmodule("hm");
  hardwaremonitor = check_and_cast<HardwareMonitor *>(hm);
  myAddress = par("address");
}


void ConnectionManager::handleMessage(cMessage *msg){

    if(dynamic_cast<ConnectionSetupRequest *>(msg)!= nullptr){
        ConnectionSetupRequest *pk = check_and_cast<ConnectionSetupRequest *>(msg);
        int actual_dst = pk->getActual_destAddr();

        if(actual_dst == myAddress){
          // terminate relaying Request & start relaying ConnectionSetupResponse
          // Here actual counter part got packet
          responder_alloc_req_handler(pk);
          delete msg;
          return;
        }else{
           // relay Request to the next node
           // OR
           // stop relaying and generate RejectConnectionSetupRequest
           intermediate_alloc_req_handler(pk);
        }
    }else if(dynamic_cast<ConnectionSetupResponse *>(msg) != nullptr){
      ConnectionSetupResponse *pk = check_and_cast<ConnectionSetupResponse *>(msg);
      int actual_src = pk->getActual_srcAddr();

      if(actual_src == myAddress){
        initiator_alloc_res_handler(pk);
        delete msg;
        return;
      }else{
        intermediate_alloc_res_handler(pk);
      }

    }else if(dynamic_cast<RejectConnectionSetupRequest *>(msg)!= nullptr){
        RejectConnectionSetupRequest *pk = check_and_cast<RejectConnectionSetupRequest *>(msg);
        int actual_src = pk->getActual_srcAddr();

        if(actual_src == myAddress){
          // terminate relaying
          responder_reject_req_handler(pk);
        }else{
          // relay RejectConnectionSetupRequest 
          intermediate_reject_req_handler(pk);
        }
    }
}

// might just be 2*l
static int compute_path_division_size (int l /**< number of links (path length, number of nodes -1) */) {
  if (l > 1) {
    int hl = (l>>1);
    return compute_path_division_size(hl) + compute_path_division_size(l-hl) + 1;
  }
  return l;
}

/** Treat subpath [i:...] of length l */
static int fill_path_division (std::vector<int> path /**< Nodes on the connection setup path */,
    int i /**< Left of the subpath to consider */, int l /**< Length of the subpath */,
    int * link_left /**< Left part of the list of "links" */, int * link_right /**< Right part */,
    int * swapper /**< Swappers to create those links (might be -1 for real links) */,
    int fill_start /** [0:fill_start[ is already filled */) {
  if (l > 1) {
    int hl = (l>>1);
    fill_start = fill_path_division (path, i, hl, link_left, link_right, swapper, fill_start);
    fill_start = fill_path_division (path, i+hl, l-hl, link_left, link_right, swapper, fill_start);
    swapper[fill_start] = path.at(i+hl);
  }
  if (l > 0) {
    link_left[fill_start] = path.at(i);
    link_right[fill_start] = path.at(i+l);
    if (l == 1) swapper[fill_start] = -1;
    fill_start++;
  }
  return fill_start;
}

void ConnectionManager::initiator_alloc_res_handler(ConnectionSetupResponse *pk){

}

void ConnectionManager::intermediate_alloc_res_handler(ConnectionSetupResponse *pk){
  
}

/**request handler
 * Procedure of this function.
 * 0. The destination is the same as me.
 * 1. get Swapper information with getter.
 * 2. generate ruleset for entanglement swapping
 * 3. return connection setup response.
*/
void ConnectionManager::responder_alloc_req_handler(ConnectionSetupRequest *pk){ 
    int hop_count = pk->getStack_of_QNodeIndexesArraySize(); // the number of steps
    std::vector<int> path; // path pointer elements?
    // path from source to destination
    for (int i = 0; i<hop_count; i++) {
      path.push_back(pk->getStack_of_QNodeIndexes(i));
    }
    path.push_back(myAddress);
    int divisions = compute_path_division_size(hop_count);
    int *link_left = new int[divisions],
        *link_right = new int[divisions],
        *swapper = new int[divisions];
    // fill_path_division should yield *exactly* the anticipated number of divisions.
    if (fill_path_division(path, 0, hop_count,link_left, link_right, swapper, 0) < divisions){
      error("Something went wrong in path division computation.");
    }
      /* TODO: Remember you have link costs <3
      for(int i = 0; i<hop_count; i++){
          //The link cost is just a dummy variable (constant 1 for now and how it is set in a bad way (read from the channel but from only 1 channels from Src->BSA and ignoring BSA->Dest).
          //If you need to test with different costs, try changing the value.
          //But we need to implement actual link-tomography for this eventually.
          EV<<"\nThis is one of the stacked link costs....."<<pk->getStack_of_linkCosts(i)<<"\n";
      }
      */
    // getting swappers index as vector(This might be redundant FIXME)
    std::vector<int> swappers = {};
    for(int i=0; i<divisions;i++){
      if(swapper[i]>0){
        swappers.push_back(swapper[i]);
      }
    }
    //HACK This may be also not good way
    std::map<int, QNIC_type> qnic_types;
    int qnic_array_size = pk->getStack_of_QNICsArraySize();
    QNIC_id_pair qnic_pairs;
    for(int i=0; i<qnic_array_size; i++){
      qnic_pairs = pk->getStack_of_QNICs(i);
      EV<<"left:"<<qnic_pairs.fst.index <<"right:"<<qnic_pairs.snd.index<<"\n";
      // EV<<"left_type:"<<qnic_pairs.fst.type<<"right:"<<qnic_pairs.snd.type<<"\n";
    }

    // create Ruleset for all nodes!
    int num_resource = pk->getNumber_of_required_Bellpairs();
    int inter_midiate_node = pk->getStack_of_QNodeIndexesArraySize();
    for(int i=0; i<=inter_midiate_node;i++){
      auto itr = std::find(swappers.begin(), swappers.end(), path.at(i));
      size_t index = std::distance( swappers.begin(), itr );
      if(index != swappers.size()){
        EV<<"Im swapper!"<<path.at(i)<<"\n";
        // generate Swapping RuleSet
        // here we have to check the order of entanglement swapping
        swap_table swap_config; // swapping configurations for path[i]
        swap_config = EntanglementSwappingConfig(path.at(i), path, qnic_pairs, num_resource);
        // RuleSet* swapping_rule = new generateRuleSet_EntanglementSwapping(createUniqueId(), path[i], );
      }else{
        EV<<"Im not swapper!"<<path[i]<<"\n";
      }

    }

    error("here!");
      // 2. Create swapping tracking rules for link_left[i] and link_right[i]
      //    Right now, those might be empty. In the end they are used to make sure that the left and right
      //    nodes are correctly tracking the estimation of the state of the qubits that get swapped.
      // Whatever happens, this 'i' line is also a 'link' relationship
      // 3. Create the purification rules for link_left[i] and link_right[i]
      // Rule * purifyrule = ...;
      // Rule * discardrule = ...; // do we need it or is it hardcoded?
      // ruleset_of_link_left_i.rules.append(purifyrule);
      // ...

    ConnectionSetupResponse *pkr = new ConnectionSetupResponse;

}

/* order_of_EntanglementSwapping
 *  This function is for selecting the order of entanglment swapping
 * output <node_address, order>
 * node_address might be better using qnic index
*/
swap_table ConnectionManager::EntanglementSwappingConfig(int swapper_address, std::vector<int> path, QNIC_pair_info qnic_pair_info, int num_resources){
  // 1.recognize partner. (which node is left partner, right partner)
  // currently, if the number of node in path is decided every other node.
  // the number of nodes is even,
  // node1 -1- node2 -1- node3 -1- node4 -1- node5 --- node6
  // node1 ------------- node3 ------------- node5 --- node6
  // node1 ------2------ node3 ------2------ node5 --- node6　when the number of nodes become 4, we have to decide which first.
  // node1 -----------3----------- node4 -3- node5 --- node6
  // node1 ------------------------------------------- node6
  //  the number of nodes is odd,
  // node1 -1- node2 -1- node3 -1- node4 -1- node5 -1- node6 -1- node7
  // node1 ------------- node3 ------------- node5 ------------- node7
  // node1 ------2------ node3 ------2------ node5 ------------- node7
  swap_table swap_setting;

  int left_partner;
  int right_partner;
  int duration = 0;
  // always take neigbor nodes recursively.
  if(path.size() % 2　== 0){
    // i. take even node from top to bottom.

  }else{
  }

  // actual configurations
  // If the counterparts are decided, the order will automatically be determined.
  auto iter = std::find(path.begin(), path.end(), swapper_address);
  size_t index = std::distance(path.begin(), iter);
  if(index == 0 || index == path.size()){
    error("This shouldn't happen. node was recognized as swapper with some reason.");
  }
  int left_partner = path.at(index-1);
  int right_partner = path.at(index+1);
  swap_setting.left_partner = left_node;
  swap_setting.lres = num_resources;
  swap_setting.right_partner = right_node;
  swap_setting.rres = num_resources;
return swap_setting;

 
}






// RuleSet **ConnectionManager::generate_RuleSet( int *stack_of_QNodeIndexes,
//                                           int *stack_of_linkCosts,
//                                           QNIC_pair_info *stack_of_QNICs){
// }

void ConnectionManager::intermediate_alloc_req_handler(ConnectionSetupRequest *pk){
  int actual_dst = pk->getActual_destAddr();
  int local_qnic_address_to_actual_dst = routingdaemon->return_QNIC_address_to_destAddr(actual_dst);
  // TODO here need to check
  int src_qnic_address = routingdaemon->return_QNIC_address_to_destAddr(pk->getSrcAddr());
  if(local_qnic_address_to_actual_dst==-1){//is not found
      error("QNIC to destination not found");
  }else{
      // Use the QNIC address to find the next hop QNode,
      // by asking the Hardware Monitor (neighbor table).
      EV<<"Source : "<<pk->getSrcAddr()<<"actual_dst : "<<local_qnic_address_to_actual_dst<<"\n";
      connection_setup_inf dst_inf = hardwaremonitor->return_setupInf(local_qnic_address_to_actual_dst);
      EV << "DST_INF " << dst_inf.qnic.type << "," << dst_inf.qnic.index << "\n";
      connection_setup_inf src_inf = hardwaremonitor->return_setupInf(src_qnic_address);
      EV << "SRC_INF " << src_inf.qnic.type << "," << src_inf.qnic.index << "\n";
      int num_accumulated_nodes = pk->getStack_of_QNodeIndexesArraySize();
      int num_accumulated_costs = pk->getStack_of_linkCostsArraySize();
      int num_accumulated_pair_info = pk->getStack_of_QNICsArraySize();

      //Update information and send it to the next Qnode.
      pk->setDestAddr(dst_inf.neighbor_address);
      EV<<"NEXT DESTINATION!!!!!!!!!!!!!!!!!!!!!!!"<<dst_inf.neighbor_address<<"\n";
      pk->setSrcAddr(myAddress);
      pk->setStack_of_QNodeIndexesArraySize(num_accumulated_nodes+1);
      pk->setStack_of_linkCostsArraySize(num_accumulated_costs+1);
      pk->setStack_of_QNodeIndexes(num_accumulated_nodes, myAddress);
      pk->setStack_of_linkCosts(num_accumulated_costs, dst_inf.quantum_link_cost);
      pk->setStack_of_QNICsArraySize(num_accumulated_pair_info+1);
      QNIC_id_pair pair_info = {
          .fst = src_inf.qnic,
          .snd = dst_inf.qnic
      };
      pk->setStack_of_QNICs(num_accumulated_pair_info, pair_info);
      pair_info = pk->getStack_of_QNICs(num_accumulated_pair_info);
      EV << "PAIR_INF " << pair_info.fst.type << "," << pair_info.fst.index << " : " << pair_info.snd.type << "," << pair_info.snd.index << "\n";
      send(pk,"RouterPort$o");
  }
}

void ConnectionManager::responder_reject_req_handler(RejectConnectionSetupRequest *pk){

}

void ConnectionManager::intermediate_reject_req_handler(RejectConnectionSetupRequest *pk){

}

RuleSet* ConnectionManager::generateRuleSet_EntanglementSwapping(unsigned int RuleSet_id,int owner,
int left_node, QNIC_type lqnic_type, int lqnic_index, int lres,
int right_node, QNIC_type rqnic_type, int rqnic_index, int rres){
    int rule_index = 0;
    std::vector<int> partners = {left_node, right_node};
    RuleSet* EntanglementSwapping = new RuleSet(RuleSet_id, owner, partners);
    Rule* SwappingRule = new Rule(RuleSet_id, rule_index);
    Condition* Swap_condition = new Condition();
    Clause* resource_clause_left = new EnoughResourceClauseLeft(left_node, 1);
    Clause* resource_clause_right = new EnoughResourceClauseRight(right_node, 1);
    Swap_condition->addClause(resource_clause_left);
    Swap_condition->addClause(resource_clause_right);
    SwappingRule->setCondition(Swap_condition);
    quisp::rules::Action* swap_action = new SwappingAction(RuleSet_id, rule_index, left_node, lqnic_type, lqnic_index, lres, right_node, rqnic_type, rqnic_index, rres);
    SwappingRule->setAction(swap_action);
    EntanglementSwapping->addRule(SwappingRule);
    rule_index++;

    return EntanglementSwapping;
}

unsigned long ConnectionManager::createUniqueId(){
    std::string time = SimTime().str();
    std::string address = std::to_string(myAddress);
    std::string random = std::to_string(intuniform(0,10000000));
    std::string hash_seed = address+time+random;
    std::hash<std::string> hash_fn;
    size_t  t = hash_fn(hash_seed);
    unsigned long RuleSet_id = static_cast<long>(t);
    std::cout<<"Hash is "<<hash_seed<<", t = "<<t<<", long = "<<RuleSet_id<<"\n";
    return RuleSet_id;
}

} // namespace modules
} // namespace quisp


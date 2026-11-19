#include "cachelet/policies/belady_policy.hpp"

Cachelet::BeladyPolicy::BeladyPolicy(std::vector<std::size_t> next_accessed) :
    next_accessed(next_accessed){}

void Cachelet::BeladyPolicy::on_access(const std::string &key){

}
void Cachelet::BeladyPolicy::on_insert(const std::string &key){

}
void Cachelet::BeladyPolicy::on_remove(const std::string &key, Cachelet::BeladyPolicy::RemoveReason remove_reason){

}
std::string Cachelet::BeladyPolicy::victim(){
    
}
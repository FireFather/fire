#include "chrono.h"
#include <cmath>
#include "main.h"
#include "uci.h"

void timecontrol::init(const search_param& limit,const side me,const int ply){
  start_time_=now();
  optimal_time_=maximum_time_=limit.time[me];
  const int maxmoves=limit.moves_to_go?std::min(limit.moves_to_go,moves_horizon_):moves_horizon_;
  const double move_importance=calc_move_importance(ply)*move_importance_factor_;
  double other_moves_importance=0;
  int64_t available=static_cast<int64_t>(limit.time[me])-move_overhead;
  for(int n=1;n<=maxmoves;++n){
    const double ratio1=move_importance/(move_importance+other_moves_importance);
    double ratio2=max_ratio_*move_importance/(max_ratio_*move_importance+other_moves_importance);
    double ratio3=(move_importance+steal_ratio_*other_moves_importance)/
      (move_importance+other_moves_importance);
    int64_t t1=std::llround(static_cast<double>(available)*ratio1);
    int64_t t2=std::llround(static_cast<double>(available)*std::min(ratio2,ratio3));
    optimal_time_=std::min(optimal_time_,t1);
    maximum_time_=std::min(maximum_time_,t2);
    other_moves_importance+=calc_move_importance(ply+2*n);
    available+=static_cast<int64_t>(limit.inc[me])-move_overhead;
  }
  optimal_time_=std::clamp(optimal_time_,minimum_time,maximum_time_);
  maximum_time_=std::max(maximum_time_,minimum_time);
  if(uci_ponder){
    optimal_time_+=static_cast<int64_t>(optimal_time_*ponder_bonus_ratio);
    optimal_time_=std::min(optimal_time_,maximum_time_);
  }
}

void timecontrol::adjustment_after_ponder_hit(){
  const int64_t new_max_time=maximum_time_+elapsed();
  optimal_time_=optimal_time_*new_max_time/maximum_time_;
}

double timecontrol::calc_move_importance(const int ply){
  double factor=1.0;
  if(ply>ply_min_&&ply<ply_max_){
    const double diff=static_cast<double>(ply)-base_moves_;
    factor=factor_base_-ply_factor_*diff*diff;
  }
  return factor*std::pow(1+std::exp((ply-x_shift_)/x_scale_),-skew_);
}

int64_t timecontrol::elapsed() const{
  return std::chrono::duration_cast<std::chrono::milliseconds>(now()-start_time_).count();
}

timecontrol time_control;

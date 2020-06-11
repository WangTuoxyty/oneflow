#include "oneflow/core/job_completer/optimizer.h"
#include <re2/re2.h>
#include "oneflow/core/job_completer/optimizer_registry.h"

namespace oneflow {

void GenerateOptimizerOpConfWrapperStruct::Call(const VariableOp& var_op,
                                                const ParallelConf& parallel_conf,
                                                JobBuilder* job_builder,
                                                const LogicalBlobId& diff_lbi_of_var_out) const {
  (*func_)(var_op, parallel_conf, job_builder, diff_lbi_of_var_out);
}

void GenerateOptimizerOpConfIf(const VariableOp& var_op, const ParallelConf& parallel_conf,
                               JobBuilder* job_builder, const LogicalBlobId& diff_lbi_of_var_out) {
  if (job_builder->job().job_conf().train_conf().has_optimizer()) {
    OptimizerRegistry::LookupAndBuild(job_builder->job().job_conf().train_conf().optimizer(),
                                      var_op, parallel_conf, diff_lbi_of_var_out,
                                      job_builder->job().job_conf().train_conf());
  } else {
    const auto& train_conf = GlobalJobDesc().job_conf().train_conf();
    auto optimizer_case = train_conf.model_update_conf().normal_mdupdt_case();
    auto* obj = NewObj<GenerateOptimizerOpConfWrapperStruct>(optimizer_case);
    obj->Call(var_op, parallel_conf, job_builder, diff_lbi_of_var_out);
  }
}

void AddOptimizerOpConf(const OpGraph& op_graph, JobBuilder* job_builder,
                        const HashMap<LogicalBlobId, LogicalBlobId>& lbi2diff_lbi) {
  op_graph.ForEachNode([&](OpNode* op_node) {
    const VariableOp* var_op = dynamic_cast<const VariableOp*>(&op_node->op());
    if (var_op == nullptr) { return; }
    if (lbi2diff_lbi.find(var_op->BnInOp2Lbi(var_op->SoleObn())) == lbi2diff_lbi.end()) { return; }

    LogicalBlobId diff_lbi_of_var_out = lbi2diff_lbi.at(var_op->BnInOp2Lbi(var_op->SoleObn()));
    const auto& parallel_desc = op_node->parallel_desc();
    GenerateOptimizerOpConfIf(*var_op, parallel_desc.parallel_conf(), job_builder,
                              diff_lbi_of_var_out);
  });
}

template<typename T>
void ConstructMdUpdtOpConf(const VariableOp& op, const LogicalBlobId& diff_lbi_of_var_out,
                           JobBuilder* job_builder, T* mdupdt_op_conf) {
  const auto& train_conf = job_builder->job().job_conf().train_conf();
  *mdupdt_op_conf->mutable_user_conf() = train_conf.model_update_conf();
  mdupdt_op_conf->set_model_diff(GenLogicalBlobName(diff_lbi_of_var_out));
  mdupdt_op_conf->set_model(GenLogicalBlobName(op.BnInOp2Lbi("out")));
  mdupdt_op_conf->set_train_step(train_conf.train_step_lbn());
  const std::string& primary_lr_lbn = train_conf.primary_lr_lbn();
  const std::string& secondary_lr_lbn = train_conf.secondary_lr_lbn();
  if (op.op_conf().variable_conf().model_name() == "weight") {
    mdupdt_op_conf->set_learning_rate(primary_lr_lbn);
  } else if (op.op_conf().variable_conf().model_name() == "bias") {
    mdupdt_op_conf->set_learning_rate(secondary_lr_lbn);
  } else {
    mdupdt_op_conf->set_learning_rate(primary_lr_lbn);
  }
  if (train_conf.model_update_conf().has_weight_decay_conf()) {
    const WeightDecayConf& weight_decay_conf = train_conf.model_update_conf().weight_decay_conf();
    std::function<bool(const std::string& op_name)> WeightDecayFilter;
    if (weight_decay_conf.has_includes()) {
      WeightDecayFilter = [&](const std::string& op_name) {
        return std::any_of(
            weight_decay_conf.includes().pattern().cbegin(),
            weight_decay_conf.includes().pattern().cend(),
            [&](const std::string& pattern) { return RE2::PartialMatch(op_name, pattern); });
      };
    } else if (weight_decay_conf.has_excludes()) {
      WeightDecayFilter = [&](const std::string& op_name) {
        return !std::any_of(
            weight_decay_conf.excludes().pattern().cbegin(),
            weight_decay_conf.excludes().pattern().cend(),
            [&](const std::string& pattern) { return RE2::PartialMatch(op_name, pattern); });
      };
    } else {
      WeightDecayFilter = [&](const std::string& op_name) { return true; };
    }
    if (WeightDecayFilter(op.op_name())) {
      mdupdt_op_conf->set_weight_decay(weight_decay_conf.weight_decay_rate());
    }
  }
}

#define INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(T)                                  \
  template void ConstructMdUpdtOpConf<T>(const VariableOp& op,                     \
                                         const LogicalBlobId& diff_lbi_of_var_out, \
                                         JobBuilder* job_builder, T* mdupdt_op_conf)

INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(NaiveModelUpdateOpConf);
INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(MomentumModelUpdateOpConf);
INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(RMSPropModelUpdateOpConf);
INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(LARSModelUpdateOpConf);
INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(AdamModelUpdateOpConf);
INSTANTIATE_CONSTRUCTOR_MDUPDT_OP_CONF(LazyAdamModelUpdateOpConf);

}  // namespace oneflow

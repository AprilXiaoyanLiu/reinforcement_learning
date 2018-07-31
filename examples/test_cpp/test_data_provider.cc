#include "test_data_provider.h"

#include "utility/data_buffer.h"
#include "ranking_event.h"

#include <sstream>

test_data_provider::test_data_provider(const std::string& experiment_name, unsigned int threads, unsigned int examples, unsigned int actions, bool _is_float_reward)
  : uuids(threads, std::vector<std::string>(examples))
  , contexts(threads, std::vector<std::string>(examples))
  , rewards(threads, std::vector<std::string>(examples))
  , reward_flag(threads, std::vector<bool>(examples))
  , is_float_reward(_is_float_reward)
{
  const std::string action_features = get_action_features(actions);
  for (unsigned int t = 0; t < threads; ++t) {
    for (unsigned int i = 0; i < examples; ++i) {
      uuids[t][i] = create_uuid(experiment_name, t, i);
      contexts[t][i] = create_context_json(create_features(t, i), action_features);
      reward_flag[t][i] = (i % 10 == 0);
      rewards[t][i] = create_json_reward(t, i);
    }
  }
}

std::string test_data_provider::create_uuid(const std::string& experiment_name, unsigned int thread_id, unsigned int example_id) const {
  std::ostringstream oss;
  oss << experiment_name << "-" << thread_id << "-" << example_id;
  return oss.str();
}

std::string test_data_provider::get_action_features(unsigned int count) const {
  std::ostringstream oss;
  oss << R"("_multi": [ )";
  for (unsigned int i = 0; i + 1 < count; ++i) {
    oss << R"({ "TAction":{"topic":"topic_)" << i << R"("} }, )";
  }
  oss << R"({ "TAction":{"topic":"topic_)" << (count - 1) << R"("} } ])";
  return oss.str();
}

std::string test_data_provider::create_features(unsigned int thread_id, unsigned int example_id) const {
  std::ostringstream oss;
  oss << R"("GUser":{)";
  oss << R"("f_int":)" << thread_id << R"(,)";
  oss << R"("f_float":)" << float(example_id) + 0.5 << R"(,)";
  oss << R"("f_string":")" << "s_" << thread_id;
  oss << R"("})";
  return oss.str();
}

std::string test_data_provider::create_json_reward(unsigned int thread_id, unsigned int example_id) const {
  std::ostringstream oss;
  oss << R"({"Reward":)" << get_reward(thread_id, example_id) << R"(,"CustomRewardField":)" << get_reward(thread_id, example_id) + 1 << "}";
  return oss.str();
}

std::string test_data_provider::create_context_json(const std::string& cntxt, const std::string& action) const {
  std::ostringstream oss;
  oss << "{ " << cntxt << ", " << action << " }";
  return oss.str();
}

float test_data_provider::get_reward(unsigned int thread_id, unsigned int example_id) const {
  return is_rewarded(thread_id, example_id) ? (thread_id  + example_id) : 0;
}

const char* test_data_provider::get_reward_json(unsigned int thread_id, unsigned int example_id) const {
  return rewards[thread_id][example_id].c_str();
}


const char* test_data_provider::get_uuid(unsigned int thread_id, unsigned int example_id) const {
  return uuids[thread_id][example_id].c_str();
}

const char* test_data_provider::get_context(unsigned int thread_id, unsigned int example_id) const {
  return contexts[thread_id][example_id].c_str();
}

bool test_data_provider::is_rewarded(unsigned int thread_id, unsigned int example_id) const {
  return reward_flag[thread_id][example_id];
}

void test_data_provider::log(unsigned int thread_id, unsigned int example_id, const reinforcement_learning::ranking_response& response, std::ostream& logger) const {
  size_t action_id;
  response.get_choosen_action_id(action_id);
  float prob = 0;
  for (auto it = response.begin(); it != response.end(); ++it) {
    if ((*it).action_id == action_id) {
      prob = (*it).probability;
    }
  }

  reinforcement_learning::utility::data_buffer buffer;
  logger << R"({"_label_cost":)" << -get_reward(thread_id, example_id) << R"(,"_label_probability":)" << prob << R"(,"_label_Action":)" << (action_id + 1) << R"(,"_labelIndex":)" << action_id << ",";

  if (is_rewarded(thread_id, example_id)) {
    if (is_float_reward)
      reinforcement_learning::outcome_event::serialize(buffer, get_uuid(thread_id, example_id), get_reward(thread_id, example_id));
    else
      reinforcement_learning::outcome_event::serialize(buffer, get_uuid(thread_id, example_id), get_reward_json(thread_id, example_id));

    logger << R"("o":[)" << buffer.str() << "],";
    buffer.reset();
  }

  reinforcement_learning::ranking_event::serialize(buffer, get_uuid(thread_id, example_id), get_context(thread_id, example_id), response);
  const std::string buffer_str = buffer.str();
  logger << buffer_str.substr(1, buffer_str.length() - 1) << std::endl;
}

int test_data_provider::report_outcome(reinforcement_learning::live_model* rl, unsigned int thread_id, unsigned int example_id, reinforcement_learning::api_status* status) const {
  if (is_float_reward)
    return rl->report_outcome(get_uuid(thread_id, example_id), get_reward(thread_id, example_id), status);
  return rl->report_outcome(get_uuid(thread_id, example_id), get_reward_json(thread_id, example_id), status);
}
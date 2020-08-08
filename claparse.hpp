//
// Created by cerussite on 7/22/20.
//

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace claparse {

namespace detail {

template <class Iterator>
void JoinTo(std::ostream& os, Iterator first, Iterator last, const std::string& delimiter) {
  auto itr = first;
  if (itr == last) {
    return;
  }
  os << *itr;
  itr++;
  std::for_each(itr, last, [&os, &delimiter](const std::string& s) { os << delimiter << s; });
}

template <class Key, class Value>
void JoinMapKeyTo(std::ostream& os, const std::map<Key, Value>& map, const std::string& delimiter) {
  auto itr = std::begin(map);
  auto last = std::end(map);
  if (itr == last) {
    return;
  }
  os << itr->first;
  itr++;
  std::for_each(itr, last, [&os, &delimiter](const std::pair<Key, Value>& s) { os << delimiter << s.first; });
}

template <class StringT>
void AppendToVector(std::vector<std::string>& vec, StringT&& s) {
  vec.emplace_back(std::forward<StringT>(s));
}

template <class StringT, class... StringsT>
void AppendToVector(std::vector<std::string>& vec, StringT&& s, StringsT&&... strings) {
  vec.emplace_back(std::forward<StringT>(s));
  AppendToVector(std::forward<StringsT>(strings)...);
}

template <class... StringsT>
std::vector<std::string> FoldVector(StringsT&&... strings) {}

}  // namespace detail

namespace engine {

class ParseState {};

class ArgumentNames {
 private:
  std::vector<std::string> names_;

 public:
  explicit ArgumentNames(std::vector<std::string> names) : names_(std::move(names)) {}
  template <class Iterator>
  ArgumentNames(Iterator first, Iterator last) : names_(first, last) {}

 public:
  const std::string& at(std::size_t n) const { return names_.at(n); }

 public:
  auto begin() const -> decltype(std::begin(names_)) { return std::begin(names_); }
  auto end() const -> decltype(std::end(names_)) { return std::end(names_); }

 public:
  bool contains(const std::string& name) const {
    return std::any_of(std::begin(names_), std::end(names_), [&name](const std::string& n) { return n == name; });
  }

 public:
  const std::string& shortName() const {
    if (names_[0].front() != '-') {
      return names_[0];
    }
    for (const auto& name : names_) {
      if (name.size() == 2 && name[1] != '-') {
        return name;
      }
    }

    return names_[0];
  }

  const std::string& longName() {
    if (names_[0].front() != '-') {
      return names_[0];
    }

    for (const auto& name : names_) {
      if (name.size() > 2 && name[1] == '-') {
        return name;
      }
    }

    return names_[0];
  }
};

}  // namespace engine

namespace actions {

class Action {
 public:
  virtual void parse(engine::ParseState&) = 0;
};

class Flag : public Action {
 public:
  static std::shared_ptr<Action> Make() { return std::make_shared<Flag>(); }

  void parse(engine::ParseState&) override {}
};

class Value : public Action {
 public:
  static std::shared_ptr<Action> Make() { return std::make_shared<Value>(); }

  void parse(engine::ParseState&) override {}
};

}  // namespace actions

class Argument {
 private:
  engine::ArgumentNames names_;
  std::shared_ptr<actions::Action> action_ = actions::Value::Make();
  std::shared_ptr<std::string> help_ = nullptr;

 public:
  template <class Iterable>
  explicit Argument(Iterable iterable) : names_(std::begin(iterable), std::end(iterable)) {}

  Argument(std::initializer_list<std::string> il) : names_(std::begin(il), std::end(il)) {}

  Argument(const Argument&) = default;
  Argument(Argument&&) noexcept = default;

  Argument& operator=(const Argument&) = default;
  Argument& operator=(Argument&&) noexcept = default;

  ~Argument() = default;

 public:
  Argument& action(std::shared_ptr<actions::Action> action) {
    action_ = std::move(action);
    return *this;
  }
  Argument& asFlag() { return action(actions::Flag::Make()); }
  Argument& asValue() { return action(actions::Value::Make()); }

 public:
  Argument& help(std::string help_string) {
    help_ = std::make_shared<std::string>(std::move(help_string));
    return *this;
  }

 public:
  void formatUsage(std::ostream& os) const { os << names().shortName(); }

  void formatHelp(std::ostream& os) const {}

  std::string formatHelp() const {
    std::stringstream ss;
    formatHelp(ss);
    return ss.str();
  }

  const engine::ArgumentNames& names() const { return names_; }
};

class ArgumentParser {
 private:
  std::string program_name_;
  std::string description_;
  std::shared_ptr<std::string> epilogue_;

  std::map<std::string, std::shared_ptr<ArgumentParser>> sub_commands_;
  std::vector<Argument> arguments_;

 public:
  ArgumentParser(std::string program_name, std::string description)
      : program_name_(std::move(program_name)), description_(std::move(description)), epilogue_() {}

  ArgumentParser(std::string program_name, std::string description, std::string epilogue)
      : program_name_(std::move(program_name)),
        description_(std::move(description)),
        epilogue_(std::make_shared<std::string>(std::move(epilogue))) {}

 public:
  template <class... StringsT>
  Argument& addArgument(StringsT&&... strings) {}

  Argument& addArgument(std::vector<std::string> name) {
    arguments_.emplace_back(std::move(name));
    return arguments_.back();
  }

  void addArgument(Argument args) { arguments_.emplace_back(std::move(args)); }

  void addArgument(std::vector<std::string> name, std::shared_ptr<actions::Action> action, std::string help) {
    addArgument(Argument(std::move(name)).action(std::move(action)).help(std::move(help)));
  }

 public:
  ArgumentParser& addSubCommand(std::string command, std::string description) {
    auto argParser = std::make_shared<ArgumentParser>(command, std::move(description));
    sub_commands_[std::move(command)] = argParser;
    return *argParser;
  }

 public:
  void formatUsage(std::ostream& os) const {
    os << "usage: " << program_name_;
    if (!sub_commands_.empty()) {
      os << " {";
      detail::JoinMapKeyTo(os, sub_commands_, ",");
      os << "}";
    }
  }

  void formatHelp(std::ostream& os) const {}
};

}  // namespace claparse

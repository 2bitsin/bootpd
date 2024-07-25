#include "arguments.hpp"

#include <iostream>

arguments::arguments(std::vector<std::string_view> _args)
: m_args{ std::move(_args) }
{
  std::string_view last_key;
  for (auto&& arg : m_args)
  {
    if (arg.size() > 1 && arg[0] == '-')   
    {
      m_flags.insert(arg);
      last_key = arg;
      continue;
    }   
    m_values[last_key].push_back(arg);
  }
}


arguments::arguments(int argc, char** argv)
: arguments{{ argv, argv + argc }}
{}

auto arguments::has(std::string_view _key) const  -> bool
{
  return m_flags.count(_key) > 0;
}

auto arguments::value(std::string_view& _value, std::string_view _key) const -> bool
{
  if (m_values.count(_key) > 0)
  {
    _value = m_values.at(_key).front();
    return true;
  }
  return false;
}

auto arguments::values(std::string_view _key) const -> const std::vector<std::string_view>&
{
  return m_values.at(_key);
}

auto arguments::value_or(std::string_view _key, std::string_view _default) const -> std::string_view
{
  if (m_values.count(_key) >0)
  {
    return m_values.at(_key).front();
  }
  return _default;
}
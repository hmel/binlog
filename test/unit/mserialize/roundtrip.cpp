#include "custom_array.hpp"

#include <mserialize/deserialize.hpp>
#include <mserialize/serialize.hpp>

#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm> // equal
#include <cmath> // isnan
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <vector>

namespace {

using arithmetic_types = boost::mpl::list<
  bool,
  std::int8_t, std::int16_t, std::int32_t, std::int64_t,
  std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
  float, double, long double
>;

using float_types = boost::mpl::list<
  float, double, long double
>;

template <typename T>
using sequence_types = boost::mpl::list<
  T[10], std::array<T, 10>,
  std::deque<T>, std::forward_list<T>,
  std::list<T>, std::vector<T>,
  test::CustomArray<T, 10>
>;

template <typename T>
using var_size_sequence_types = boost::mpl::list<
  std::deque<T>, std::forward_list<T>,
  std::list<T>, std::vector<T>
>;

// In tests, do not use stringstream directly,
// to make sure tested code only accesses
// members of the specific concept.

struct OutputStream
{
  std::stringstream& stream;

  OutputStream& write(const char* buf, std::streamsize size)
  {
    stream.write(buf, size);
    return *this;
  }
};

struct InputStream
{
  std::stringstream& stream;

  InputStream& read(char* buf, std::streamsize size)
  {
    stream.read(buf, size);
    return *this;
  }
};

template <typename In, typename Out>
void roundtrip_into(const In& in, Out& out)
{
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);

  // serialize
  OutputStream ostream{stream};
  mserialize::serialize(in, ostream);

  // deserialize
  InputStream istream{stream};
  mserialize::deserialize(out, istream);
}

template <typename T>
T roundtrip(const T& in)
{
  T out;
  roundtrip_into(in, out);
  return out;
}

// Boost.Test has trouble comparing/printing nested containers
template <typename A, typename B, typename Cmp>
bool deep_container_equal(const A& a, const B& b, Cmp cmp)
{
  using std::begin;
  using std::end;
  return std::equal(begin(a), end(a), begin(b), end(b), cmp);
}

auto container_equal()
{
  return [](auto& a, auto& b)
  {
    using std::begin;
    using std::end;
    BOOST_TEST(a == b, boost::test_tools::per_element());
    return std::equal(begin(a), end(a), begin(b), end(b));
  };
}

} // namespace

BOOST_AUTO_TEST_SUITE(MserializeRoundtrip)

BOOST_AUTO_TEST_CASE_TEMPLATE(arithmetic_min_max, T, arithmetic_types)
{
  // min
  {
    const T in = std::numeric_limits<T>::max();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // max
  {
    const T in = std::numeric_limits<T>::min();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(float_spec, T, float_types)
{
  // lowest
  {
    const T in = std::numeric_limits<T>::lowest();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // Negative 0
  {
    const T in{-0.0};
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // -Inf
  if (std::numeric_limits<T>::has_infinity)
  {
    const T in = T{-1.} * std::numeric_limits<T>::infinity();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // +Inf
  if (std::numeric_limits<T>::has_infinity)
  {
    const T in = std::numeric_limits<T>::infinity();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // Quiet NaN
  if (std::numeric_limits<T>::has_quiet_NaN)
  {
    const T in = std::numeric_limits<T>::quiet_NaN();
    const T out = roundtrip(in);
    BOOST_TEST(std::isnan(out));
  }

  // Signaling NaN
  if (std::numeric_limits<T>::has_signaling_NaN)
  {
    const T in = std::numeric_limits<T>::signaling_NaN();
    const T out = roundtrip(in);
    BOOST_TEST(std::isnan(out));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(sequence_of_int, T, sequence_types<int>)
{
  /*const*/ T in{0,1,2,3,4,5,6,7,8,9};
  T out; // NOLINT(cppcoreguidelines-pro-type-member-init)
  roundtrip_into(in, out);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(empty_sequence_of_int, T, var_size_sequence_types<int>)
{
  /*const*/ T in;
  T out{1, 2, 3};
  roundtrip_into(in, out);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(sequence_of_vector_of_int, T, sequence_types<std::vector<int>>)
{
  using V = std::vector<int>;
  const T in{
    V{}, V{1,2,3}, V{4,5,6},
    V{7}, V{8,9}, V{10,11,12,13,14,15,16},
    V{17,18,19,20}, V{21,21}, V{22}, V{}
  };
  T out;
  roundtrip_into(in, out);

  BOOST_TEST(deep_container_equal(
    in, out, container_equal()
  ));
}

BOOST_AUTO_TEST_CASE(sequence_cross)
{
  const std::vector<std::deque<std::array<int, 3>>> in{
    { {1,2,3}, {4,5,6} },
    { {7,8,9}          },
    { {10, 11, 12}, {13,14,15}, {16,17,18}}
  };

  std::forward_list<std::list<int>> out[3];
  roundtrip_into(in, out);

  BOOST_TEST(deep_container_equal(
    in, out,
    [](auto& c, auto& d)
    {
      return deep_container_equal(c, d, container_equal());
    }
  ));
}


BOOST_AUTO_TEST_CASE(vector_of_bool)
{
  const std::vector<bool> in{true, false, false, true, true, false};
  std::vector<bool> out{false, false};
  roundtrip_into(in, out);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE(sequence_size_mismatch)
{
  const std::array<int, 3> in{1,2,3};
  std::array<int, 6> out{0,0,0,0,0,0};
  BOOST_CHECK_THROW(
    roundtrip_into(in, out),
    std::runtime_error
  );
}

BOOST_AUTO_TEST_CASE(string)
{
  // empty
  {
    const std::string in;
    const std::string out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // not-empty
  {
    const std::string in = "foobar";
    const std::string out = roundtrip(in);
    BOOST_TEST(in == out);
  }
}

BOOST_AUTO_TEST_CASE(errorOnEof)
{
  int out;
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);
  BOOST_CHECK_THROW(
    mserialize::deserialize(out, stream),
    std::exception
  );
}

BOOST_AUTO_TEST_CASE(errorOnIncomplete)
{
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);
  mserialize::serialize(std::int16_t{123}, stream);

  std::int32_t out;
  BOOST_CHECK_THROW(
    mserialize::deserialize(out, stream),
    std::exception
  );
}

BOOST_AUTO_TEST_SUITE_END()
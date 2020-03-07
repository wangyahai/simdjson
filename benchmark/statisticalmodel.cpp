#include <iostream>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include "simdjson.h"
#ifdef __linux__
#include "linux-perf-events.h"
#endif

size_t count_nonasciibytes(const uint8_t *input, size_t length) {
  size_t count = 0;
  for (size_t i = 0; i < length; i++) {
    count += input[i] >> 7;
  }
  return count;
}

size_t count_backslash(const uint8_t *input, size_t length) {
  size_t count = 0;
  for (size_t i = 0; i < length; i++) {
    count += (input[i] == '\\') ? 1 : 0;
  }
  return count;
}

struct stat_s {
  size_t integer_count;
  size_t float_count;
  size_t string_count;
  size_t backslash_count;
  size_t non_ascii_byte_count;
  size_t object_count;
  size_t array_count;
  size_t null_count;
  size_t true_count;
  size_t false_count;
  size_t byte_count;
  size_t structural_indexes_count;
  bool valid;
};

using stat_t = struct stat_s;

stat_t simdjson_compute_stats(const simdjson::padded_string &p) {
  stat_t answer;
  simdjson::ParsedJson pj = simdjson::build_parsed_json(p);
  answer.valid = pj.is_valid();
  if (!answer.valid) {
    return answer;
  }
  answer.backslash_count =
      count_backslash(reinterpret_cast<const uint8_t *>(p.data()), p.size());
  answer.non_ascii_byte_count = count_nonasciibytes(
      reinterpret_cast<const uint8_t *>(p.data()), p.size());
  answer.byte_count = p.size();
  answer.integer_count = 0;
  answer.float_count = 0;
  answer.object_count = 0;
  answer.array_count = 0;
  answer.null_count = 0;
  answer.true_count = 0;
  answer.false_count = 0;
  answer.string_count = 0;
  answer.structural_indexes_count = pj.n_structural_indexes;
  size_t tape_idx = 0;
  uint64_t tape_val = pj.doc.tape[tape_idx++];
  uint8_t type = (tape_val >> 56);
  size_t how_many = 0;
  assert(type == 'r');
  how_many = tape_val & JSON_VALUE_MASK;
  for (; tape_idx < how_many; tape_idx++) {
    tape_val = pj.doc.tape[tape_idx];
    // uint64_t payload = tape_val & JSON_VALUE_MASK;
    type = (tape_val >> 56);
    switch (type) {
    case 'l': // we have a long int
      answer.integer_count++;
      tape_idx++; // skipping the integer
      break;
    case 'u': // we have a long uint
      answer.integer_count++;
      tape_idx++; // skipping the integer
      break;
    case 'd': // we have a double
      answer.float_count++;
      tape_idx++; // skipping the double
      break;
    case 'n': // we have a null
      answer.null_count++;
      break;
    case 't': // we have a true
      answer.true_count++;
      break;
    case 'f': // we have a false
      answer.false_count++;
      break;
    case '{': // we have an object
      answer.object_count++;
      break;
    case '}': // we end an object
      break;
    case '[': // we start an array
      answer.array_count++;
      break;
    case ']': // we end an array
      break;
    case '"': // we have a string
      answer.string_count++;
      break;
    default:
      break; // ignore
    }
  }
  return answer;
}

int main(int argc, char *argv[]) {
#ifndef _MSC_VER
  int c;
  while ((c = getopt(argc, argv, "")) != -1) {
    switch (c) {

    default:
      abort();
    }
  }
#else
  int optind = 1;
#endif
  if (optind >= argc) {
    std::cerr << "Reads json, prints stats. " << std::endl;
    std::cerr << "Usage: " << argv[0] << " <jsonfile>" << std::endl;

    exit(1);
  }
  const char *filename = argv[optind];
  if (optind + 1 < argc) {
    std::cerr << "warning: ignoring everything after " << argv[optind + 1]
              << std::endl;
  }
  auto [p, error] = simdjson::padded_string::load(filename);
  if (error) {
    std::cerr << "Could not load the file " << filename << std::endl;
    return EXIT_FAILURE;
  }
  stat_t s = simdjson_compute_stats(p);
  if (!s.valid) {
    std::cerr << "not a valid JSON" << std::endl;
    return EXIT_FAILURE;
  }

  printf("# integer_count float_count string_count backslash_count "
         "non_ascii_byte_count object_count array_count null_count true_count "
         "false_count byte_count structural_indexes_count ");
#ifdef __linux__
  printf("  stage1_cycle_count stage1_instruction_count  stage2_cycle_count "
         " stage2_instruction_count  stage3_cycle_count "
         "stage3_instruction_count  ");
#else
  printf("(you are not under linux, so perf counters are disaabled)");
#endif
  printf("\n");
  printf("%zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu ", s.integer_count,
         s.float_count, s.string_count, s.backslash_count,
         s.non_ascii_byte_count, s.object_count, s.array_count, s.null_count,
         s.true_count, s.false_count, s.byte_count, s.structural_indexes_count);
#ifdef __linux__
  simdjson::document::parser parser;
  const simdjson::implementation &stage_parser = *simdjson::active_implementation;
  bool allocok = parser.allocate_capacity(p.size());
  if (!allocok) {
    std::cerr << "failed to allocate memory" << std::endl;
    return EXIT_FAILURE;
  }
  const uint32_t iterations = p.size() < 1 * 1000 * 1000 ? 1000 : 50;
  std::vector<int> evts;
  evts.push_back(PERF_COUNT_HW_CPU_CYCLES);
  evts.push_back(PERF_COUNT_HW_INSTRUCTIONS);
  LinuxEvents<PERF_TYPE_HARDWARE> unified(evts);
  unsigned long cy1 = 0, cy2 = 0;
  unsigned long cl1 = 0, cl2 = 0;
  std::vector<unsigned long long> results;
  results.resize(evts.size());
  for (uint32_t i = 0; i < iterations; i++) {
    unified.start();
    // The default template is simdjson::architecture::NATIVE.
    bool isok = (stage_parser.stage1((const uint8_t *)p.data(), p.size(), parser, false) == simdjson::SUCCESS);
    unified.end(results);

    cy1 += results[0];
    cl1 += results[1];

    unified.start();
    isok = isok && (stage_parser.stage2((const uint8_t *)p.data(), p.size(), parser) == simdjson::SUCCESS);
    unified.end(results);

    cy2 += results[0];
    cl2 += results[1];
    if (!isok) {
      std::cerr << "failure?" << std::endl;
    }
  }
  printf("%f %f %f %f ", cy1 * 1.0 / iterations, cl1 * 1.0 / iterations,
         cy2 * 1.0 / iterations, cl2 * 1.0 / iterations);
#endif // __linux__
  printf("\n");
  return EXIT_SUCCESS;
}

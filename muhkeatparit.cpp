#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <chrono>

#if defined(__AVX__) || defined(__SSE4_2__)
#include <nmmintrin.h>
#define popcount _mm_popcnt_u32
#else
// On my desktop, this version is extremely slow on windows: slow one (2.8s) vs _mm_popcnt_u32 (0.1s).
// On my laptop, in linux the difference is:                 slow one (0.6s) vs _mm_popcnt_u32 (0.4s).
#include <bitset>
#include <climits>
int _popcount(int n) {
  std::bitset<sizeof(size_t) * CHAR_BIT> b(n);
  return b.count();
}
#define popcount _popcount
#endif

namespace std
{
  using WordIndex = int;
  using WordPair = pair<WordIndex, WordIndex>;

  template <> struct hash<WordPair>
  {
    size_t operator()(const WordPair & x) const
    {
      return hash<int>()(x.first ^ x.second);
    }
  };
  template <> struct equal_to<WordPair>
  {
    bool operator()(const WordPair & x, const WordPair & y) const
    {
      hash<WordPair> h;
      return h(x) == h(y);
    }
  };
}

using namespace std;

size_t filesizeInBytes(const char* filename)
{
  fstream in(filename, ios::in);
  auto begin = in.tellg();
  in.seekg(0, ios::end);
  return static_cast<size_t>(in.tellg() - begin);
}

vector<string> splitToWordVector(string& s)
{
  std::string delimiter = " ";
  unordered_set<string> extractedUniqueWords(s.size()/7);
  size_t start = 0;
  size_t end = s.find(delimiter);
  while (end != std::string::npos)
  {
    extractedUniqueWords.insert(s.substr(start, end - start));
    start = end + delimiter.length();
    end = s.find(delimiter, start);
  }
  extractedUniqueWords.insert(s.substr(start, end - start));

  vector<string> uniqueWords(extractedUniqueWords.size());
  uniqueWords.assign(extractedUniqueWords.begin(), extractedUniqueWords.end());
  return uniqueWords;
}

inline char handleSpecialChar(char& c)
{
  if (c == -92 || c == -124) // ä
    c = '{';
  else if (c == -74 || c == -106) // ö
    c = '|';
  else if (c == -91 || c == -123) // å
    c = '}';
  else if (c == '\n') // have space separating each word.
    c = ' ';
  return c;
}

void cleanString(string& s)
{
  transform(s.begin(), s.end(), s.begin(), [](char c)
  {
    return handleSpecialChar(c);
  });
  transform(s.begin(), s.end(), s.begin(), ::tolower); // doesn't work utf-8
  s.erase(remove(s.begin(), s.end(), '\''), s.end()); // "vaa'asta" -> "vaaasta"
  transform(s.begin(), s.end(), s.begin(), [](char c) // unnecessary chars -> '\0'
  {
    if (c < 'a' && c != ' ')
      return '\0';
    return c;
  });
  s.erase(remove(s.begin(), s.end(), '\0'), s.end());
}

string utf8fy(string word)
{
  string output;
  unordered_map<char, string> replace = { {'{', u8"ä"}, {'|', u8"ö"}, {'}', u8"å"} };
  for (auto&& character : word)
  {
    string possibleChar = replace[character];
    if (possibleChar.size() > 0)
      output += possibleChar;
    else
      output += character;
  }
  return output;
}

// Finding the muhkeimmat parit. Should be pretty good ratio with speed against code quality.
int main(int argc, char* argv[])
{
  auto start_time = chrono::high_resolution_clock::now();
  if (argc < 2)
  {
    cerr << "Must give filepath as parameter." << endl;
    return 0;
  }
  decltype(argv[1]) filename = argv[1];
  size_t total_size = filesizeInBytes(filename);
  ifstream file(filename, ifstream::binary);

  if (!file.good())
  {
    cerr << "Given file \"" << filename << "\" was bad. Bailing out!" << endl;
    return 0;
  }

  string fileContents(total_size, '\0');
  file.read(&fileContents[0], total_size);
  cleanString(fileContents);
  auto allWords = splitToWordVector(fileContents);

  using WordIndex = int;
  using Bitfield = unsigned int;
  using Uniqueness = int;
  using WordMetaInfo = tuple<Bitfield, Uniqueness, WordIndex>;
  vector<WordMetaInfo> wordMuhkeaInfos;

  auto makeBitfield = [](const string& word)
  {
    Bitfield ret = 0;
    for (auto& character : word)
    {
      ret |= (1 << (character - 'a'));
    }
    return ret;
  };

  WordIndex i = -1;
  for (auto&& word : allWords)
  {
    Bitfield bitfield = makeBitfield(word);
    wordMuhkeaInfos.emplace_back(make_tuple(makeBitfield(word), popcount(bitfield), ++i));
  }

  sort(wordMuhkeaInfos.begin(), wordMuhkeaInfos.end(), [](const WordMetaInfo& tuple1,const WordMetaInfo& tuple2)
  {
    return get<1>(tuple1) > get<1>(tuple2);
  });

  using WordPair = pair<WordIndex, WordIndex>;
  unordered_set<WordPair> bestPairs;

  Uniqueness currentMuhkeinPair = 0;
  Uniqueness MuhkeinWord = get<1>(wordMuhkeaInfos[0]);
  Uniqueness minRequiredWordMuhkeus = 0;

  for (auto&& word1 : wordMuhkeaInfos)
  {
    auto& word1Muhkeus = get<1>(word1);
    auto& word1Bitfield = get<0>(word1);

    if (word1Muhkeus < minRequiredWordMuhkeus)
    {
      break;
    }

    for (auto&& word2 : wordMuhkeaInfos)
    {
      auto& word2Muhkeus = get<1>(word2);
      auto& word2Bitfield = get<0>(word2);
      auto pairBitfield = word1Bitfield | word2Bitfield;
      auto pairMuhkeus = popcount(pairBitfield);

      if (word1Muhkeus + word2Muhkeus < currentMuhkeinPair)
      {
        break;
      }
      else if (pairMuhkeus > currentMuhkeinPair)
      {
        bestPairs.clear();
        bestPairs.insert(make_pair(get<2>(word1), get<2>(word2)));
        currentMuhkeinPair = pairMuhkeus;
        minRequiredWordMuhkeus = currentMuhkeinPair - MuhkeinWord;
      }
      else if (pairMuhkeus == currentMuhkeinPair)
      {
        bestPairs.insert(make_pair(get<2>(word1), get<2>(word2)));
      }
    }
  }
  auto end_time = chrono::high_resolution_clock::now();
  // cout << "Found " << bestPairs.size() << " pairs:" << endl;
  for (auto&& pair : bestPairs)
  {
    cout << utf8fy(allWords[pair.first]) << " \t " << utf8fy(allWords[pair.second]) << endl;
  }
  auto end_time2 = chrono::high_resolution_clock::now();
  chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(end_time - start_time);
  cout << "before print " << time_span.count() << " seconds." << endl;
  time_span = chrono::duration_cast<chrono::duration<double>>(end_time2 - start_time);
  cout << "whole thing " << time_span.count() << " seconds." << endl;
  return 0;
}

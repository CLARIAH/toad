/*
  Copyright (c) 2010 - 2016
  ILK  Tilburg University
  CLST Radboud University

  This file is part of toad

  toad is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  toad is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  For questions and suggestions, see:
      https://github.com/LanguageMachines/toad/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/
#include<getopt.h>
#include<iostream>
#include<fstream>
#include<vector>
#include<set>
#include<map>
#include<string>
#include<cstdlib>
#include "timbl/TimblAPI.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "frog/Frog.h"
#include "frog/mbma_mod.h"

using namespace std;

TiCC::LogStream *theErrLog = new TiCC::LogStream(cerr);

TiCC::Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/frog/";
static string configFileName = configDir + "frog.cfg";

Mbma myMbma(theErrLog);
set<string> lexicon;
set<string> mor_lexicon;

UnicodeString UnicodeStringFromS( const string& s, const string& enc = "UTF8" ){
  return UnicodeString( s.c_str(), s.length(), enc.c_str() );
}

template< typename T >
std::ostream& operator<< ( std::ostream& os, const std::set<T>& s ){
  os << "{";
  typename std::set<T>::const_iterator it = s.begin();
  while ( it != s.end() ){
    os << *it;
    ++it;
    if ( it != s.end() )
      os << ",";
  }
  os << "}";
  return os;
}

bool isException( const string& s ){
  if ( s.size() < 2 )
    return true;
  return false;
}

void usage(){
  cerr << "checkmbma [-h] [-m] [-S<limit>]" << endl;
  cerr << "check mbma-merged.lex for inconsistencies" << endl;
  cerr << "\t -m signal unknow morphemes too. (a lot!) " << endl;
}

void check_word( const string& word, bool doMor ){
  UnicodeString us( word.c_str() );
  UnicodeString ls = us;
  ls.toLower();
  if ( us != ls )
    return;
  myMbma.Classify( ls );
  vector<vector<string> > ana = myMbma.getResult();
  set<int> fails;
  for ( size_t i=0; i < ana.size(); i++ ){
    bool lem_found = false;
    for ( size_t j=0; j < ana[i].size(); j++ ){
      string mor = ana[i][j];
      mor = TiCC::lowercase(mor);
      if ( mor == word ){
	lem_found = true;
	break;
      }
      else if ( lexicon.find(mor) != lexicon.end() ){
	//	  cerr << "found lemma " << mor << endl;
	lem_found = true;
      }
      else if ( doMor
		&& mor.size() != 1
		&& j != 0
		&& mor_lexicon.find(mor) == mor_lexicon.end() ){
	//	  cerr << "NOT found mor " << mor << endl;
	fails.insert(j);
      }
    }
    if ( !lem_found ){
      using TiCC::operator<<;
      cerr << "UNK LEMMA " << word << " - " << ana[i] << endl;
    }
    else if ( fails.size() > 0 ){
      using TiCC::operator<<;
      cerr << "UNK MOR ";
      set<int>::const_iterator it = fails.begin();
      while ( it != fails.end() ){
	cerr << "[" << ana[i][*it] << "] ";
	++it;
      }
      cerr << word << " - " << ana[i] << endl;
    }
  }
}

int main(int argc, char * const argv[] ) {
  string lexname = "mbma-merged.lex";
  string inpname ;
  bool doMor = false;
  bool testSonar = false;
  string debug;
  size_t limit = 0;
  int opt;
  while ( (opt = getopt( argc, argv, "d:hmS:t:")) != -1 ){
    switch ( opt ){
    case 'm': doMor = true; break;
    case 'd':
      debug = optarg;
      break;
    case 'S':
      testSonar = true;
      limit = TiCC::stringTo<size_t>( optarg );
      break;
    case 't':
      inpname = optarg;
      break;
    case 'h': usage(); return EXIT_SUCCESS; break;
    default: usage(); return EXIT_FAILURE;
    }
  }

  ifstream bron( lexname.c_str() );
  if ( !bron ){
    cerr << "could not open mbma file '" << lexname << "'" << endl;
    return EXIT_FAILURE;
  }

  map<string,size_t> test_lex;
  cout << "building a lexicon from " << lexname << endl;
  string line;
  while ( getline(bron, line ) ){
    vector<string> parts;
    int num = TiCC::split_at( line, parts, " " );
    if ( num < 2 ){
      cerr << "Problem in line '" << line << "' (to short?)" << endl;
      continue;
    }
    UnicodeString word = UnicodeStringFromS( parts[0] );
    word.toLower();
    if ( word.length() != num-1 ){
      cerr << "Problem in line '" << line << "' (" << word.length()
	   << " letters, but got " << num-1 << " morphemes)" << endl;
      continue;
    }
    lexicon.insert( folia::UnicodeToUTF8( word ) );
  }
  cout << "found " << lexicon.size() << " words " << endl;
  bron.close();
  bron.open( "sonar.lemmas" );
  while ( getline(bron, line ) ){
    if ( line.empty() )
      continue;
    vector<string> vec;
    TiCC::split( line, vec );
    lexicon.insert( vec[0] );
  }
  bron.close();
  cout << "added sonar lemmas, size is now: " << lexicon.size() << " words " << endl;
  bron.open( "known.lemmas" );
  while ( getline(bron, line ) ){
    if ( line.empty() )
      continue;
    vector<string> vec;
    TiCC::split( line, vec );
    lexicon.insert( vec[0] );
  }
  bron.close();
  cout << "added known lemmas, size is now: " << lexicon.size() << " words " << endl;
  bron.open( "known.morphs" );
  while ( getline(bron, line ) ){
    if ( line.empty() )
      continue;
    vector<string> vec;
    TiCC::split( line, vec );
    mor_lexicon.insert( vec[0] );
  }
  bron.close();
  cout << "found " << mor_lexicon.size() << " known morphemes." << endl;
  bron.close();
  if ( testSonar ){
    bron.open( "sonar.words" );
    while ( getline(bron, line ) ){
      if ( line.empty() )
	continue;
      vector<string> vec;
      if ( TiCC::split( line, vec ) == 4 ){
	size_t freq;
	if ( !TiCC::stringTo<size_t>( vec[1], freq ) ){
	  cerr << "illegal int in " << line << endl;
	  continue;
	}
	if ( freq > limit ){
	  test_lex[ vec[0] ] = freq;
	}
      }
    }
    bron.close();
    cout << "read " << test_lex.size() << " test words from sonar.words" << endl;
  }

  configuration.fill( configFileName );
  if ( !debug.empty() ){
    configuration.setatt( "debug", debug, "mbma" );
  }
  myMbma.init( configuration );
  if ( testSonar ){
    cout << "checking the morphemes in sonar.lemmas " << endl;
    map<string,size_t>::const_iterator it = test_lex.begin();
    while ( it != test_lex.end() ){
      check_word( it->first, doMor );
      ++it;
    }
  }
  else if ( !inpname.empty() ){
    bron.open( inpname.c_str() );
    cout << "checking the morphemes in " << inpname << endl;
    while ( getline(bron, line ) ){
      vector<string> parts;
      TiCC::split( line, parts );
      check_word( parts[0], doMor );
    }
  }
  else {
    bron.open( lexname.c_str() );
    cout << "checking the morphemes in " << lexname << endl;
    while ( getline(bron, line ) ){
      vector<string> parts;
      TiCC::split_at( line, parts, " " );
      check_word( parts[0], doMor );
    }
  }
  return 0;
}

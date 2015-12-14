/*
  Copyright (c) CLS 2015-2016

  Froggen: A Generator for Frog datafiles.

  Centre for Language Studies, Radboud University Nijmegen

  Comments and bug-reports are welcome at our issue tracker at
  https://github.com/LanguageMachines/timblserver/issues
  or by mailing
  lamasoftware (at ) science.ru.nl
*/


#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "timbl/TimblAPI.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"

using namespace std;

int debug = 0;
#define HISTORY    20

UnicodeString UnicodeFromS( const string& s, const string& enc = "UTF8" ){
  return UnicodeString( s.c_str(), s.length(), enc.c_str() );
}

string UnicodeToUTF8( const UnicodeString& s ){
  string result;
  s.toUTF8String(result);
  return result;
}

void usage(){
  cerr << "froggen -T tagggedcorpus [-l lemmalist] [-e encoding] [-O outputdir]"
       << endl;
}

void fill_lemmas( istream& is,
		  multimap<UnicodeString, multimap<UnicodeString, UnicodeString>>& lems,
		  const string& enc ){
  string line;
  while ( getline( is, line ) ){
    if ( line == "<utt>" )
      continue;
    vector<string> parts;
    size_t num = TiCC::split( line, parts );
    if ( num != 3 ){
      cerr << "wrong inputline (should be 3 parts)" << endl;
      cerr << "'" << line << "'" << endl;
      exit( EXIT_FAILURE );
    }
    vector<UnicodeString> uparts(3);
    uparts[0] = UnicodeFromS( parts[0], enc );
    uparts[1] = UnicodeFromS( parts[1], enc );
    uparts[2] = UnicodeFromS( parts[2], enc );
    auto it = lems.lower_bound( uparts[0] );
    if ( it == lems.upper_bound( uparts[0] ) ){
      it = lems.insert( make_pair( uparts[0], multimap<UnicodeString,UnicodeString>() ) );
      it->second.insert( make_pair(uparts[1], uparts[2]) );
    }
    else {
      auto it2 = it->second.lower_bound( uparts[1] );
      if ( it2 == it->second.upper_bound( uparts[1] ) ){
	// so this lemma not yet done for this word
	it2 = it->second.insert( make_pair( uparts[1],uparts[2]) );
      }
      else {
	bool done = false;
	for ( ;!done && (it2 != it->second.upper_bound( uparts[1] )); ++it2 ){
	  // look if this tageis done for this word/lemma combi
	  if ( it2->second == uparts[2] ){
	    // YES
	    done = true;
	  }
	}
	if ( !done ){
	  it->second.insert( make_pair( uparts[1],uparts[2]) );
	}
      }
    }
  }
}

void fill_corpus( istream& is,
		  multimap<UnicodeString, multimap<UnicodeString, UnicodeString>>& lems,
		  const string& enc ){
  string line;
  while ( getline( is, line ) ){
    vector<string> parts;
    size_t num = TiCC::split( line, parts );
    if ( num != 3 ){
      cerr << "wrong inputline (should be 3 parts)" << endl;
      cerr << "'" << line << "'" << endl;
      exit( EXIT_FAILURE );
    }
    vector<UnicodeString> uparts(3);
    uparts[0] = UnicodeFromS( parts[0], enc );
    uparts[1] = UnicodeFromS( parts[1], enc );
    uparts[2] = UnicodeFromS( parts[2], enc );
    auto it = lems.find( uparts[0] );
    if ( it == lems.end() ){
      it = lems.insert( make_pair( uparts[0], multimap<UnicodeString,UnicodeString>() ) );

    }
    it->second.insert( make_pair(uparts[1], uparts[2]) );
  }
}


void create_mblem_trainfile( multimap<UnicodeString, multimap<UnicodeString, UnicodeString>>& data, const string& filename ){
  ofstream os( filename );
  if ( !os ){
    cerr << "couldn't create mblem datafile" << filename << endl;
    exit( EXIT_FAILURE );
  }
  UnicodeString lastinstance;
  UnicodeString outLine;
  for ( const auto& it : data ){
    if ( !outLine.isEmpty() ){
      os << UnicodeToUTF8(outLine) << endl;
      outLine.remove();
    }
    UnicodeString wordform = it.first;
    for ( const auto& it2 : it.second ){
      UnicodeString lemma  = it2.first;
      UnicodeString tag = it2.second;
      UnicodeString memwordform = wordform;
      UnicodeString prefixed;
      UnicodeString deleted;
      UnicodeString inserted;
      size_t ident=0;
      while ( ident < wordform.length() &&
	      ident < lemma.length() &&
	      wordform[ident]==lemma[ident] )
	ident++;
      if ( ident < wordform.length() ) {
	for ( int i=ident; i< wordform.length(); i++) {
	  deleted += wordform[i];
	}
      }
      if ( ident< lemma.length() ) {
	for ( int i=ident; i< lemma.length(); i++) {
	  inserted += lemma[i];
	}
      }
      if ( debug ){
	cerr << " word " << wordform << ", lemma " << lemma
	     << ", prefix " << prefixed
	     << ", insert " << inserted
	     << ", delete " << deleted << endl;
      }
      UnicodeString instance;
      // print instance
      for ( int i=0; i<HISTORY; i++) {
	int j= memwordform.length()-HISTORY+i;
	if (j<0)
	  instance += "= ";
	else {
	  UChar uc = memwordform[j];
	  instance += uc;
	  instance += " ";
	}
      }
      if ( instance != lastinstance ){
	outLine += instance;
	lastinstance = instance;
      }
      else {
	outLine += "|";
      }
      outLine += tag;
      if ( !prefixed.isEmpty() )
	outLine += "+P" + prefixed;
      if ( !deleted.isEmpty() )
	outLine += "+D" + deleted;
      if ( !inserted.isEmpty() )
	outLine += "+I" + inserted;
    }
  }
  if ( !outLine.isEmpty() )
    os << UnicodeToUTF8(outLine) << endl;
  cout << "created a mblm trainingsfile: " << filename << endl;
}

void train_mblem( const string& inputfile, const string& outputfile ){
  string timblopts = "-a1 -w2 +vS";
  cout << "Timbl: Start training " << inputfile << " with Options: "
       << timblopts << endl;
  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( inputfile );
  timbl.WriteInstanceBase( outputfile );
  cout << "Timbl: Done, stored instancebase : " << outputfile << endl;
}

int main( int argc, char * const argv[] ) {
  TiCC::CL_Options opts("T:l:e:O:","");
  opts.parse_args( argc, argv );

  string corpusname;
  string outputdir;
  string lemmaname;
  string encoding = "UTF-8";

  if ( !opts.extract( 'T', corpusname ) ){
    cerr << "Missing a corpus!, (-T option)" << endl;
    exit( EXIT_FAILURE );
  }
  opts.extract( 'l', lemmaname );
  opts.extract( 'O', outputdir );
  opts.extract( 'e', encoding );

  multimap<UnicodeString,multimap<UnicodeString,UnicodeString>> data;

  ifstream corpus( corpusname);
  if ( !corpus ){
    cerr << "could not open corpus file '" << corpusname << "'" << endl;
    return EXIT_FAILURE;
  }
  cout << "start reading lemmas from: " << corpusname << endl;
  fill_lemmas( corpus, data, encoding );
  if ( !lemmaname.empty() ){
    cout << "start reading extra lemmas from: " << lemmaname << endl;
    ifstream is( lemmaname);
    if ( !is ){
      cerr << "could not open lemma list '" << lemmaname << "'" << endl;
      return EXIT_FAILURE;
    }
    fill_lemmas( is, data, encoding );
  }
  // for( const auto& it : data ){
  //   for ( const auto& it2 : it.second ){
  //     cout << it.first << " " << it2.first << " " << it2.second << endl;
  //   }
  // }
  string mblemdatafilename = "mblem.data";
  string mblemoutputfilename = "mblem.tree";
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    mblemdatafilename = outputdir + mblemdatafilename;
    mblemoutputfilename = outputdir + mblemoutputfilename;
  }
  create_mblem_trainfile( data, mblemdatafilename );
  train_mblem( mblemdatafilename, mblemoutputfilename );
  return EXIT_SUCCESS;
}
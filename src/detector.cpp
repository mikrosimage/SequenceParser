#include "detector.hpp"
#include "Sequence.hpp"
#include "Folder.hpp"
#include "File.hpp"
#include "utils.hpp"

#include "detail/analyze.hpp"
#include "detail/FileNumbers.hpp"
#include "detail/FileStrings.hpp"
#include "commonDefinitions.hpp"

#include <boost/regex.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <set>


namespace sequenceParser {

using detail::FileNumbers;
using detail::FileStrings;
using detail::SeqIdHash;
namespace bfs = boost::filesystem;

boost::ptr_vector<File> fileInDirectory( const std::string& directory, const EMaskOptions desc )
{
	std::vector<std::string> filters;
	return fileInDirectory( directory, filters, desc );
}

boost::ptr_vector<File> fileInDirectory( const std::string& dir, std::vector<std::string>& filters, const EMaskOptions desc )
{
	boost::ptr_vector<File> outputFiles;
	std::string tmpDir( dir );
	std::string filename;
	
	if( !detectDirectoryInResearch( tmpDir, filters, filename ) )
	{
		return outputFiles;
	}
	
	const std::vector<boost::regex> reFilters = convertFilterToRegex( filters, desc );

	// variables for sequence detection
	typedef boost::unordered_map<FileStrings, std::vector<FileNumbers>, SeqIdHash> SeqIdMap;
	bfs::path directory( tmpDir );
	SeqIdMap sequences;

	// temporary variables, should be inside the loop but here for optimization.
	FileStrings tmpStringParts; // an object uniquely identify a sequence
	FileNumbers tmpNumberParts; // the vector of numbers inside one filename

	// for all files in the directory
	for( bfs::directory_iterator iter( directory ), itEnd; iter != itEnd; ++iter )
	{
		// clear previous infos
		tmpStringParts.clear();
		tmpNumberParts.clear(); // (clear but don't realloc the vector inside)

		if( isNotFilter( iter->path(), reFilters, filename, desc ) )
		{
			// it's a file or a file of a sequence
			if( filenameIsNotFilter( iter->path().filename().string(), reFilters ) &&
			    ( ( filename.size() && filename == iter->path().filename().string() ) || !filename.size() ) ) // filtering of entries with filters strings
			{
				// if at least one number detected
				if( decomposeFilename( iter->path().filename().string(), tmpStringParts, tmpNumberParts, desc ) )
				{
					// need to construct sequence to detect file with a pattern but with only one image
					const SeqIdMap::iterator it( sequences.find( tmpStringParts ) );
					if( it != sequences.end() ) // is already in map
					{
						// append the vector of numbers
						sequences.at( tmpStringParts ).push_back( tmpNumberParts );
					}
					else
					{
						// create an entry in the map
						std::vector<FileNumbers> li;
						li.push_back( tmpNumberParts );
						sequences.insert( SeqIdMap::value_type( tmpStringParts, li ) );
					}
				}
				else
				{
					if( ! bfs::is_directory( directory / iter->path().filename().string() ) )
					{
						outputFiles.push_back( new File( directory, iter->path().filename().string(), desc ) );
					}
				}
			}
		}
	}
	// add sequences in the output vector
	BOOST_FOREACH( SeqIdMap::value_type & p, sequences )
	{
		if( p.second.size() == 1 )
		{
			std::string filename = p.first[0];
			filename += p.second[0].getString(0);
			filename += p.first[1];
			//std::cout << "FILENAME = " << filename << std::endl;
			outputFiles.push_back( new File( directory, filename, desc ) );
		}
	}

	return outputFiles;
}

boost::ptr_vector<Sequence> sequenceInDirectory( const std::string& directory, const EMaskOptions desc )
{
	std::vector<std::string> filters;
	return sequenceInDirectory( directory, filters, desc );
}

boost::ptr_vector<Sequence> sequenceInDirectory( const std::string& dir, std::vector<std::string>& filters, const EMaskOptions desc )
{
	boost::ptr_vector<Sequence> outputSequences;
	std::string tmpDir( dir );
	std::string filename;
	
	if( !detectDirectoryInResearch( tmpDir, filters, filename ) )
	{
		return outputSequences;
	}

	const std::vector<boost::regex> reFilters = convertFilterToRegex( filters, desc );

	// variables for sequence detection
	typedef boost::unordered_map<FileStrings, std::vector<FileNumbers>, SeqIdHash> SeqIdMap;
	bfs::path directory( tmpDir );
	SeqIdMap sequences;
	FileStrings tmpStringParts; // an object uniquely identify a sequence
	FileNumbers tmpNumberParts; // the vector of numbers inside one filename

	// for all files in the directory
	bfs::directory_iterator itEnd;
	for( bfs::directory_iterator iter( directory ); iter != itEnd; ++iter )
	{
		// clear previous infos
		tmpStringParts.clear();
		tmpNumberParts.clear(); // (clear but don't realloc the vector inside)

		if( isNotFilter( iter->path(), reFilters, filename, desc ) )
		{
			// it's a file or a file of a sequence
			if( filenameIsNotFilter( iter->path().filename().string(), reFilters ) ) // filtering of entries with filters strings
			{
				// if at least one number detected
				if( decomposeFilename( iter->path().filename().string(), tmpStringParts, tmpNumberParts, desc ) )
				{
					const SeqIdMap::iterator it( sequences.find( tmpStringParts ) );
					if( it != sequences.end() ) // is already in map
					{
						// append the vector of numbers
						sequences.at( tmpStringParts ).push_back( tmpNumberParts );
					}
					else
					{
						// create an entry in the map
						std::vector<FileNumbers> li;
						li.push_back( tmpNumberParts );
						sequences.insert( SeqIdMap::value_type( tmpStringParts, li ) );
					}
				}
			}
		}
	}
	// add sequences in the output vector

	BOOST_FOREACH( SeqIdMap::value_type & p, sequences )
	{
		const std::vector<Sequence> ss = buildSequences( directory, p.first, p.second, desc );

		BOOST_FOREACH( const std::vector<Sequence>::value_type & s, ss )
		{
			// don't detect sequence of directories
			if( !bfs::is_directory( s.getAbsoluteFirstFilename() ) )
			{
				if( !( s.getNbFiles() == 1 ) ) // if it's a sequence of 1 file, it isn't a sequence but only a file
				{
					outputSequences.push_back( new Sequence( directory, s, desc ) );
				}
			}
		}
	}

	return outputSequences;
}

boost::ptr_vector<Sequence> sequenceFromFilenameList( const std::vector<boost::filesystem::path>& filenames, const EMaskOptions desc )
{
	std::vector<std::string> filters; // @todo as argument !
	boost::ptr_vector<Sequence> outputSequences;

	const std::vector<boost::regex> reFilters = convertFilterToRegex( filters, desc );

	// variables for sequence detection
	typedef boost::unordered_map<FileStrings, std::vector<FileNumbers>, SeqIdHash> SeqIdMap;
	SeqIdMap sequences;
	FileStrings tmpStringParts; // an object uniquely identify a sequence
	FileNumbers tmpNumberParts; // the vector of numbers inside one filename

	// for all files in the directory

	BOOST_FOREACH( const boost::filesystem::path& iter, filenames )
	{
		// clear previous infos
		tmpStringParts.clear();
		tmpNumberParts.clear(); // (clear but don't realloc the vector inside)

		if( !( iter.filename().string()[0] == '.' ) || ( desc & eMaskOptionsDotFile ) ) // if we ask to show hidden files and if it is hidden
		{
			// it's a file or a file of a sequence
			if( filenameIsNotFilter( iter.filename().string(), reFilters ) ) // filtering of entries with filters strings
			{
				// if at least one number detected
				if( decomposeFilename( iter.filename().string(), tmpStringParts, tmpNumberParts, desc ) )
				{
					const SeqIdMap::iterator it( sequences.find( tmpStringParts ) );
					if( it != sequences.end() ) // is already in map
					{
						// append the vector of numbers
						sequences.at( tmpStringParts ).push_back( tmpNumberParts );
					}
					else
					{
						// create an entry in the map
						std::vector<FileNumbers> li;
						li.push_back( tmpNumberParts );
						sequences.insert( SeqIdMap::value_type( tmpStringParts, li ) );
					}
				}
			}
		}
	}

	boost::filesystem::path directory; ///< @todo filter by directories, etc.

	// add sequences in the output vector
	BOOST_FOREACH( SeqIdMap::value_type & p, sequences )
	{
		//std::cout << "FileStrings: " << p.first << std::endl;
		const std::vector<Sequence> ss = buildSequences( directory, p.first, p.second, desc );

		BOOST_FOREACH( const std::vector<Sequence>::value_type & s, ss )
		{
			// don't detect sequence of directories
			if( !bfs::is_directory( s.getAbsoluteFirstFilename() ) )
			{
				if( !( s.getNbFiles() == 1 ) ) // if it's a sequence of 1 file, it isn't a sequence but only a file
				{
					outputSequences.push_back( new Sequence( directory, s, desc ) );
				}
			}
		}
	}

	return outputSequences;
}

boost::ptr_vector<FileObject> fileAndSequenceInDirectory( const std::string& directory, const EMaskOptions desc )
{
	std::vector<std::string> filters;
	return fileAndSequenceInDirectory( directory, filters, desc );
}

boost::ptr_vector<FileObject> fileAndSequenceInDirectory( const std::string& dir, std::vector<std::string>& filters, const EMaskOptions desc )
{
	boost::ptr_vector<FileObject> output;
	boost::ptr_vector<FileObject> outputFiles;
	boost::ptr_vector<FileObject> outputSequences;
	std::string tmpDir( dir );
	std::string filename;
	
	if( ! detectDirectoryInResearch( tmpDir, filters, filename ) )
	{
		return output;
	}

	const std::vector<boost::regex> reFilters = convertFilterToRegex( filters, desc );

	// variables for sequence detection
	typedef boost::unordered_map<FileStrings, std::vector<FileNumbers>, SeqIdHash> SeqIdMap;
	bfs::path directory( tmpDir );
	SeqIdMap sequences;
	FileStrings tmpStringParts; // an object uniquely identify a sequence
	FileNumbers tmpNumberParts; // the vector of numbers inside one filename

	// for all files in the directory
	bfs::directory_iterator itEnd;
	for( bfs::directory_iterator iter( directory ); iter != itEnd; ++iter )
	{
		// clear previous infos
		tmpStringParts.clear();
		tmpNumberParts.clear(); // (clear but don't realloc the vector inside)

		if( isNotFilter( iter->path(), reFilters, filename, desc ) )
		{
			// it's a file or a file of a sequence
			if( filenameIsNotFilter( iter->path().filename().string(), reFilters ) ) // filtering of entries with filters strings
			{
				// if at least one number detected
				if( decomposeFilename( iter->path().filename().string(), tmpStringParts, tmpNumberParts, desc ) )
				{
					const SeqIdMap::iterator it( sequences.find( tmpStringParts ) );
					if( it != sequences.end() ) // is already in map
					{
						// append the vector of numbers
						sequences.at( tmpStringParts ).push_back( tmpNumberParts );
					}
					else
					{
						// create an entry in the map
						std::vector<FileNumbers> li;
						li.push_back( tmpNumberParts );
						sequences.insert( SeqIdMap::value_type( tmpStringParts, li ) );
					}
				}
				else
				{
					outputFiles.push_back( new File( directory, iter->path().filename().string(), desc ) );
				}
			}
		}
	}
	// add sequences in the output vector

	BOOST_FOREACH( SeqIdMap::value_type & p, sequences )
	{
		if( p.second.size() == 1 )
		{
			std::string filename = p.first[0];
			filename += p.second[0].getString(0);
			filename += p.first[1];
			//std::cout << "FILENAME = " << filename << std::endl;
			outputFiles.push_back( new File( directory, filename, desc ) );
		}
		else
		{
			const std::vector<Sequence> ss = buildSequences( directory, p.first, p.second, desc );

			BOOST_FOREACH( const std::vector<Sequence>::value_type & s, ss )
			{
				// don't detect sequence of directories
				if( !bfs::is_directory( s.getAbsoluteFirstFilename() ) )
				{
					if( s.getNbFiles() == 1 ) // if it's a sequence of 1 file, it isn't a sequence but only a file
					{
						outputFiles.push_back( new File( directory, s.getFirstFilename(), desc ) );
					}
					else
					{
						outputSequences.push_back( new Sequence( directory, s, desc ) );
					}
				}
			}
		}
	}

	output.transfer( output.begin(), outputFiles );
	output.transfer( output.begin(), outputSequences );
	return output;
}

boost::ptr_vector<Folder> folderInDirectory( const std::string& directory, const EMaskOptions desc )
{
	std::vector<std::string> filters;
	return folderInDirectory( directory, filters, desc );
}

boost::ptr_vector<Folder> folderInDirectory( const std::string& dir, std::vector<std::string>& filters, const EMaskOptions desc )
{
	boost::ptr_vector<Folder> outputFolders;
	bfs::path directory;
	std::string filename; // never fill in this case

	if( bfs::exists( dir ) )
	{
		if( bfs::is_directory( dir ) )
		{
			directory = bfs::path( dir );
		}
		else
		{
			return outputFolders;
		}
	}
	else
	{
		return outputFolders;
	}

	const std::vector<boost::regex> reFilters = convertFilterToRegex( filters, desc );

	// for all files in the directory
	bfs::directory_iterator itEnd;
	for( bfs::directory_iterator iter( directory ); iter != itEnd; ++iter )
	{
		if( isNotFilter( iter->path(), reFilters, filename, desc ) )
		{
			// detect if is a folder
			if( bfs::is_directory( iter->status() ) )
			{
				outputFolders.push_back( new Folder( directory, iter->path().filename().string(), desc ) );
			}
		}
	}
	return outputFolders;
}

boost::ptr_vector<FileObject> fileObjectInDirectory( const std::string& directory, const EMaskType mask, const EMaskOptions desc )
{
	std::vector<std::string> filters;
	return fileObjectInDirectory( directory, filters, mask, desc );
}

boost::ptr_vector<FileObject> fileObjectInDirectory( const std::string& dir, std::vector<std::string>& filters, const EMaskType mask, const EMaskOptions desc )
{
	boost::ptr_vector<FileObject> output;
	boost::ptr_vector<FileObject> outputFolders;
	boost::ptr_vector<FileObject> outputFiles;
	boost::ptr_vector<FileObject> outputSequences;
	std::string tmpDir( dir );
	std::string filename;
	
	if( !detectDirectoryInResearch( tmpDir, filters, filename ) )
	{
		return outputFiles;
	}

	const std::vector<boost::regex> reFilters = convertFilterToRegex( filters, desc );

	// variables for sequence detection
	typedef boost::unordered_map<FileStrings, std::vector<FileNumbers>, SeqIdHash> SeqIdMap;
	bfs::path directory( tmpDir );
	SeqIdMap sequences;
	FileStrings tmpStringParts; // an object uniquely identify a sequence
	FileNumbers tmpNumberParts; // the vector of numbers inside one filename

	// for all files in the directory
	bfs::directory_iterator itEnd;
	for( bfs::directory_iterator iter( directory ); iter != itEnd; ++iter )
	{
		// clear previous infos
		tmpStringParts.clear();
		tmpNumberParts.clear(); // (clear but don't realloc the vector inside)

		if( isNotFilter( iter->path(), reFilters, filename, desc ) )
		{
			// @todo: no check that needs to ask the status of the file here (like asking if it's a folder).
			// detect if is a folder
			if( bfs::is_directory( iter->status() ) )
			{
				outputFolders.push_back( new Folder( directory, iter->path().filename().string(), desc ) );
			}
			else // it's a file or a file of a sequence
			{
				// if at least one number detected
				if( decomposeFilename( iter->path().filename().string(), tmpStringParts, tmpNumberParts, desc ) )
				{
					const SeqIdMap::iterator it( sequences.find( tmpStringParts ) );
					if( it != sequences.end() ) // is already in map
					{
						// append the vector of numbers
						sequences.at( tmpStringParts ).push_back( tmpNumberParts );
					}
					else
					{
						// create an entry in the map
						std::vector<FileNumbers> li;
						li.push_back( tmpNumberParts );
						sequences.insert( SeqIdMap::value_type( tmpStringParts, li ) );
					}
				}
				else
				{
					outputFiles.push_back( new File( directory, iter->path().filename().string(), desc ) );
				}
			}
		}
	}
	// add sequences in the output vector

	BOOST_FOREACH( SeqIdMap::value_type & p, sequences )
	{
		if( p.second.size() == 1 )
		{
			std::string filename = p.first.getId().at( 0 );
			for( size_t i = 0; i < p.second[ 0 ].size(); i++ )
			{
				filename += p.second[ 0 ].getString( i );
				filename += p.first.getId().at( i+1 );
			}
			//std::cout << "FILENAME = " << filename << std::endl;
			outputFiles.push_back( new File( directory, filename, desc ) );
		}
		else
		{
			const std::vector<Sequence> ss = buildSequences( directory, p.first, p.second, desc );

			BOOST_FOREACH( const std::vector<Sequence>::value_type & s, ss )
			{
				// don't detect sequence of directories
				if( !bfs::is_directory( s.getAbsoluteFirstFilename() ) )
				{
					if( s.getNbFiles() == 1 ) // if it's a sequence of 1 file, it isn't a sequence but only a file
					{
						outputFiles.push_back( new File( directory, s.getFirstFilename(), desc ) );
					}
					else
					{
						outputSequences.push_back( new Sequence( directory, s, desc ) );
					}
				}
				// else
				// {
				// @todo loop to declare direcotries !
				// }
			}
		}
	}

	if( mask & eMaskTypeDirectory )
	{
		output.transfer( output.end(), outputFolders );
	}
	// add files in the output vector
	if( mask & eMaskTypeFile )
	{
		output.transfer( output.end(), outputFiles );
	}
	// add sequences in the output vector
	if( mask & eMaskTypeSequence )
	{
		output.transfer( output.end(), outputSequences );
	}
	return output;
}

}
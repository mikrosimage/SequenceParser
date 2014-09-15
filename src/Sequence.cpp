#include "Sequence.hpp"

#include "detail/FileNumbers.hpp"

#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/foreach.hpp>
#include <set>

#include <ostream>


namespace sequenceParser {

namespace bfs = boost::filesystem;

/// All regex to recognize a pattern
// common used pattern with # or @
static const boost::regex regexPatternStandard( "(.*?)" // anything but without priority
												"\\[?" // if pattern is myimage[####].jpg, don't capture []
												"(#+|@+)" // we capture all # or @
												"\\]?" // possible end of []
												"(.*?)" // anything
												);
// C style pattern
static const boost::regex regexPatternCStyle( "(.*?)" // anything but without priority
											  "\\[?" // if pattern is myimage[%04d].jpg, don't capture []
											  "%([0-9]*)d" // we capture the padding value (eg. myimage%04d.jpg)
											  "\\]?" // possible end of []
											  "(.*?)" // anything
											  );
// image name
static const boost::regex regexPatternFrame( "(.*?" // anything but without priority
											 "[_\\.]?)" // if multiple numbers, the number surround with . _ get priority (eg. seq1shot04myimage.0123.jpg -> 0123)
											 "\\[?" // if pattern is myimage[0001].jpg, don't capture []
											 "([0-9]+)" // one frame number, can only be positive ( 0012 )
											 "\\]?" // possible end of []
											 "([_\\.]?" // if multiple numbers, the number surround with . _ get priority (eg. seq1shot04myimage.0123.jpg -> 0123)
											 ".*\\.?" //
											 ".*?)" // anything
											 );

// image name with negative indexes
static const boost::regex regexPatternFrameNeg( "(.*?" // anything but without priority
												"[_\\.]?)" // if multiple numbers, the number surround with . _ get priority (eg. seq1shot04myimage.0123.jpg -> 0123)
												"\\[?" // if pattern is myimage[0001].jpg, don't capture []
												"([\\-\\+]?[0-9]+)" // one frame number, can be positive or negative values ( -0012 or +0012 or 0012)
												"\\]?" // possible end of []
												"([_\\.]?" // if multiple numbers, the number surround with . _ get priority (eg. seq1shot04myimage.0123.jpg -> 0123)
												".*\\.?" //
												".*?)" // anything
												);


template<typename T>
inline T greatestCommonDivisor( T a, T b )
{
	T r;
	if( b == 0 )
		return 0;
	while( ( r = a % b ) != 0 )
	{
		a = b;
		b = r;
	}
	return b;
}


/**
 * @brief Find the biggest common step from a set of all steps.
 */
std::size_t greatestCommonDivisor( const std::set<std::size_t>& steps )
{
	if( steps.size() == 1 )
	{
		return *steps.begin();
	}
	std::set<std::size_t> allSteps;
	for( std::set<std::size_t>::const_iterator itA = steps.begin(), itB = ++steps.begin(), itEnd = steps.end(); itB != itEnd; ++itA, ++itB )
	{
		allSteps.insert( greatestCommonDivisor( *itB, *itA ) );
	}
	return greatestCommonDivisor( allSteps );
}


/**
 * @brief Extract step from a sorted vector of time values.
 */
std::size_t extractStep( const std::vector<Time>& times )
{
	if( times.size() <= 1 )
	{
		return 1;
	}
	std::set<std::size_t> allSteps;
	for( std::vector<Time>::const_iterator itA = times.begin(), itB = ++times.begin(), itEnd = times.end(); itB != itEnd; ++itA, ++itB )
	{
		allSteps.insert( *itB - *itA );
	}
	return greatestCommonDivisor( allSteps );
}


/**
 * @brief Extract step from a sorted vector of time values.
 */
std::size_t extractStep( const std::vector<detail::FileNumbers>::const_iterator& timesBegin, const std::vector<detail::FileNumbers>::const_iterator& timesEnd, const std::size_t i )
{
	if( std::distance( timesBegin, timesEnd ) <= 1 )
	{
		return 1;
	}
	std::set<std::size_t> allSteps;
	for( std::vector<detail::FileNumbers>::const_iterator itA = timesBegin, itB = boost::next(timesBegin), itEnd = timesEnd; itB != itEnd; ++itA, ++itB )
	{
		allSteps.insert( itB->getTime( i ) - itA->getTime( i ) );
	}
	return greatestCommonDivisor( allSteps );
}


std::size_t getPaddingFromStringNumber( const std::string& timeStr )
{
	if( timeStr.size() > 1 )
	{
		// if the number is signed, this charater does not count as padding.
		if( timeStr[0] == '-' || timeStr[0] == '+' )
		{
			return timeStr.size() - 1;
		}
	}
	return timeStr.size();
}


/**
 * @brief extract the padding from a vector of frame numbers
 * @param[in] timesStr vector of frame numbers in string format
 */
std::size_t extractPadding( const std::vector<std::string>& timesStr )
{
	BOOST_ASSERT( timesStr.size() > 0 );
	const std::size_t padding = getPaddingFromStringNumber( timesStr.front() );

	BOOST_FOREACH( const std::string& s, timesStr )
	{
		if( padding != getPaddingFromStringNumber( s ) )
		{
			return 0;
		}
	}
	return padding;
}


std::size_t extractPadding( const std::vector<detail::FileNumbers>::const_iterator& timesBegin, const std::vector<detail::FileNumbers>::const_iterator& timesEnd, const std::size_t i )
{
	BOOST_ASSERT( timesBegin != timesEnd );

	std::set<std::size_t> padding;
	std::set<std::size_t> nbDigits;
	
	for( std::vector<detail::FileNumbers>::const_iterator s = timesBegin;
		 s != timesEnd;
		 ++s )
	{
		padding.insert( s->getPadding(i) );
		nbDigits.insert( s->getNbDigits(i) );
	}
	
	std::set<std::size_t> pad = padding;
	pad.erase(0);
	
	if( pad.size() == 0 )
	{
		return 0;
	}
	else if( pad.size() == 1 )
	{
		return *pad.begin();
	}
	
	// @todo multi-padding !
	// need to split into multiple sequences !
	return 0;
}


/**
 * @brief return if the padding is strict (at least one frame begins with a '0' padding character).
 * @param[in] timesStr vector of frame numbers in string format
 * @param[in] padding previously detected padding
 */
bool extractIsStrictPadding( const std::vector<std::string>& timesStr, const std::size_t padding )
{
	if( padding == 0 )
	{
		return false;
	}

	BOOST_FOREACH( const std::string& s, timesStr )
	{
		if( s[0] == '0' )
		{
			return true;
		}
	}
	return false;
}


bool extractIsStrictPadding( const std::vector<detail::FileNumbers>& times, const std::size_t i, const std::size_t padding )
{
	if( padding == 0 )
	{
		return false;
	}

	BOOST_FOREACH( const detail::FileNumbers& s, times )
	{
		if( s.getString( i )[0] == '0' )
		{
			return true;
		}
	}
	return false;
}


std::string Sequence::getFilenameAt( const Time time ) const
{
	std::ostringstream o;
	if( time >= 0 )
	{
		// "prefix.0001.jpg"
		o << _prefix << std::setw( _padding ) << std::setfill( _fillCar ) << time << _suffix;
	}
	else
	{
		// "prefix.-0001.jpg" (and not "prefix.000-1.jpg")
		o << _prefix << "-" << std::setw( _padding ) << std::setfill( _fillCar ) << std::abs( (int) time ) << _suffix;
	}
	return o.str();
}


std::string Sequence::getCStylePattern() const
{
	if( getPadding() )
		return getPrefix() + "%0" + boost::lexical_cast<std::string > ( getPadding() ) + "d" + getSuffix();
	else
		return getPrefix() + "%d" + getSuffix();
}


Time Sequence::getNbFiles() const
{
	Time nbFiles = 0;
	BOOST_FOREACH(const FrameRange& frameRange, _ranges)
	{
		nbFiles += frameRange.getNbFrames();
	}
	return nbFiles;
}


bool Sequence::isIn( const std::string& filename, Time& time, std::string& timeStr )
{
	std::size_t min = _prefix.size() + _suffix.size();

	if( filename.size() <= min )
		return false;

	if( filename.substr( 0, _prefix.size() ) != _prefix || filename.substr( filename.size() - _suffix.size(), _suffix.size() ) != _suffix )
		return false;

	try
	{
		timeStr = filename.substr( _prefix.size(), filename.size() - _suffix.size() - _prefix.size() );
		time = boost::lexical_cast<Time > ( timeStr );
	}
	catch( ... )
	{
		return false;
	}
	return true;
}


EPattern Sequence::checkPattern( const std::string& pattern, const EDetection detectionOptions )
{
	if( regex_match( pattern.c_str(), regexPatternStandard ) )
	{
		return ePatternStandard;
	}
	else if( regex_match( pattern.c_str(), regexPatternCStyle ) )
	{
		return ePatternCStyle;
	}
	else if( ( detectionOptions & eDetectionNegative ) && regex_match( pattern.c_str(), regexPatternFrameNeg ) )
	{
		return ePatternFrameNeg;
	}
	else if( regex_match( pattern.c_str(), regexPatternFrame ) )
	{
		return ePatternFrame;
	}
	return ePatternNone;
}


/**
 * @brief This function creates a regex from the pattern,
 *        and init internal values.
 * @param[in] pattern
 * @param[in] accept
 * @param[out] prefix
 * @param[out] suffix
 * @param[out] padding
 * @param[out] strictPadding
 */
bool Sequence::retrieveInfosFromPattern( const std::string& filePattern, const EPattern& accept, std::string& prefix, std::string& suffix, std::size_t& padding, bool& strictPadding ) const
{
	boost::cmatch matches;
	//std::cout << filePattern << " / " << prefix << " + " << padding << " + " << suffix << std::endl;
	if( ( accept & ePatternStandard ) && regex_match( filePattern.c_str(), matches, regexPatternStandard ) )
	{
		std::string paddingStr( matches[2].first, matches[2].second );
		padding = paddingStr.size();
		strictPadding = ( paddingStr[0] == '#' );
	}
	else if( ( accept & ePatternCStyle ) && regex_match( filePattern.c_str(), matches, regexPatternCStyle ) )
	{
		std::string paddingStr( matches[2].first, matches[2].second );
		padding = paddingStr.size() == 0 ? 0 : boost::lexical_cast<std::size_t > ( paddingStr ); // if no padding value: %d -> padding = 0
		strictPadding = false;
	}
	else if( ( accept & ePatternFrame ) && regex_match( filePattern.c_str(), matches, regexPatternFrame ) )
	{
		std::string frame( matches[2].first, matches[2].second );
		// Time t = boost::lexical_cast<Time>( frame );
		padding = frame.size();
		strictPadding = false;
	}
	else if( ( accept & ePatternFrameNeg ) && regex_match( filePattern.c_str(), matches, regexPatternFrameNeg ) )
	{
		std::string frame( matches[2].first, matches[2].second );
		// Time t = boost::lexical_cast<Time>( frame );
		padding = frame.size();
		strictPadding = false;
	}
	else
	{
		// this is a file, not a sequence
		return false;
	}
	prefix = std::string( matches[1].first, matches[1].second );
	suffix = std::string( matches[3].first, matches[3].second );
	return true;
}


void Sequence::init( const std::string& prefix, const std::size_t padding, const std::string& suffix, const Time firstTime, const Time lastTime, const Time step, const bool strictPadding )
{
	_prefix = prefix;
	_padding = padding;
	_suffix = suffix;
	_ranges.clear();
	_ranges.push_back(FrameRange(firstTime, lastTime, step));
	_strictPadding = strictPadding;
}


bool Sequence::initFromPattern( const std::string& pattern, const Time firstTime, const Time lastTime, const Time step, const EPattern accept )
{
	if( !retrieveInfosFromPattern( pattern, accept, _prefix, _suffix, _padding, _strictPadding ) )
		return false; // not regognize as a pattern, maybe a still file
	_ranges.clear();
	_ranges.push_back(FrameRange(firstTime, lastTime, step));
	return true;
}


std::vector<boost::filesystem::path> Sequence::getFiles() const
{
	std::vector<boost::filesystem::path> allPaths;
	BOOST_FOREACH(const FrameRange& range, _ranges)
	{
		for( Time t = range.first; t <= range.last; t += range.step ){
			allPaths.push_back( getFilenameAt( t ) );
		}
	}
	return allPaths;
}


std::string Sequence::string() const
{
	std::ostringstream ss;
	ss << *this;
	return ss.str();
}


std::ostream& operator<<(std::ostream& os, const Sequence& sequence)
{
	os << sequence.getStandardPattern() << " [" << sequence.getFrameRanges() << "]";
	return os;
}


}

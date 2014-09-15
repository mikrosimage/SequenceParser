
inline std::string Sequence::getFirstFilename() const
{
	return getFilenameAt( getFirstTime() );
}

inline char Sequence::getPatternCharacter() const
{
	return getPadding() ? '#' : '@';
}

inline std::string Sequence::getStandardPattern() const
{
	return getPrefix() + std::string( getPadding() ? getPadding() : 1, getPatternCharacter() ) + getSuffix();
}

inline std::pair<Time, Time> Sequence::getGlobalRange() const
{
	return std::pair<Time, Time > ( getFirstTime(), getLastTime() );
}

inline Time Sequence::getFirstTime() const
{
	return _ranges.front().first;
}

inline Time Sequence::getLastTime() const
{
	return _ranges.back().last;
}

inline std::size_t Sequence::getDuration() const
{
	return getLastTime() - getFirstTime() + 1;
}

inline std::size_t Sequence::getPadding() const
{
	return _padding;
}

inline bool Sequence::isStrictPadding() const
{
	return _strictPadding;
}

inline bool Sequence::hasMissingFile() const
{
	return _ranges.size() != 1 || _ranges.front().step != 1;
}

inline std::size_t Sequence::getNbMissingFiles() const
{
	return (( getLastTime() - getFirstTime() ) + 1 ) - getNbFiles();
}

/// @brief filename without frame number

inline std::string Sequence::getIdentification() const
{
	return _prefix + _suffix;
}

inline std::string Sequence::getPrefix() const
{
	return _prefix;
}

inline std::string Sequence::getSuffix() const
{
	return _suffix;
}


/*
 * Browser.h
 *
 *  Created on: 16 fevr. 2012
 *      Author: Guillaume Chatelet
 */

#ifndef BROWSER_H_
#define BROWSER_H_

#include <sequence/BrowseItem.h>
#include <vector>

namespace sequence {
namespace parser {

std::vector<sequence::BrowseItem> browse(const char* directory, bool recursive=false);

} /* namespace parser */
} /* namespace sequence */
#endif /* BROWSER_H_ */

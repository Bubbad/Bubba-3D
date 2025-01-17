/*
 * This file is part of Bubba-3D.
 *
 * Bubba-3D is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Bubba-3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Bubba-3D. If not, see http://www.gnu.org/licenses/.
 */
#include "Logger.h"
#include "sstream"
#include <time.h>

using namespace std;

unsigned int Logger::currentLogLevel = DEBUG;
std::vector<LogHandler*> Logger::logHandlers;

void Logger::logDebug(string msg) {
	log(DEBUG, msg);
}

void Logger::logWarning(string msg) {
	log(WARNING, msg);
}

void Logger::logError(string msg) {
	log(SEVERE, msg);
}

void Logger::logInfo(string msg) {
	log(INFO, msg);
}

void Logger::log(unsigned int level, string msg) {
	if (level >= currentLogLevel) {
		for (auto logHandler : logHandlers) {
			std::stringstream ss;
			ss << levelToString(level) << "\t" << getTime().c_str() << "\t" << msg.c_str() << "\n";
			logHandler->log(ss.str());
		}
	}
}

void Logger::addLogHandler(LogHandler *logHandler) {
    logHandlers.push_back(logHandler);
}

void Logger::setLogLevel(unsigned int level) {
	currentLogLevel = level;
}

string Logger::levelToString(unsigned int level) {
	switch (level) {
		case INFO:     return "INFO";
		case DEBUG:    return "DEBUG";
		case WARNING:  return "WARNING";
		case SEVERE:   return "SEVERE";
		default: return "BAD level";
	}
}

string Logger::getTime() {
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
	return buf;
}

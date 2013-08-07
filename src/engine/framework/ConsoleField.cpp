/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 2013 Unvanquished Developers

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

#include "ConsoleField.h"
#include "CommandSystem.h"
#include "../../shared/String.h"
#include "../qcommon/q_shared.h"

//TODO: use unicode

namespace Console {

    Field::Field(): LineEditData(512) {
    }

    Field::~Field() {
        if (oldBuffer) {
            delete[] oldBuffer;
        }
    }

    void Field::HistoryPrev() {
        if (not inHistory) {
            hist = HistoryEnd();
        }

        std::string line = PrevLine(hist);

        if (line.empty()) {
            return;
        }

        if (not inHistory) {
            inHistory = true;
            oldScroll = scroll;
            oldCursor = cursor;
            oldBuffer = buffer;
            buffer = new char[bufferSize];
        }

        Q_strncpyz(buffer, line.c_str(), bufferSize);
        CursorEnd();
    }

    void Field::HistoryNext() {
        if (not inHistory) {
            return;
        }

        std::string line = NextLine(hist);
        if (not line.empty()) {
            Q_strncpyz(buffer, line.c_str(), bufferSize);
            CursorEnd();
        } else {
            delete[] buffer;
            buffer = oldBuffer;
            oldBuffer = nullptr;

            inHistory = false;
            scroll = oldScroll;
            cursor = oldCursor;
        }
    }

    void Field::RunCommand(const std::string& defaultCommand) {
        if(strlen(buffer) == 0) {
            return;
        }

        if (buffer[0] == '/' or buffer[0] == '\\') {
            AddToHistory(buffer + 1);
            Cmd::BufferCommandText(buffer + 1, Cmd::END, true);
        } else {
            AddToHistory(buffer);
            if (defaultCommand.empty()) {
                Cmd::BufferCommandText(buffer, Cmd::END, true);
            } else {
                Cmd::BufferCommandText(defaultCommand + " " + std::string(buffer), Cmd::END, true);
            }
        }

        inHistory = false;
        Clear();
    }

    void Field::AutoComplete() {
        //We want to complete in the middle of a command text with potentially multiple commands

        int slashOffset = 0;
        if (buffer[0] == '/' or buffer[0] == '\\') {
            slashOffset = 1;
        }
        std::string commandText(buffer + slashOffset);

        //Split the command text and find the command to complete
        std::vector<int> starts = Cmd::StartsOfCommands(commandText);
        if (starts.back() < strlen(buffer)) {
            starts.push_back(strlen(buffer) + 1);
        }

        int index = 0;
        for (index = starts.size(); index --> 0;) {
            if (starts[index] < cursor) {
                break;
            }
        }
        int commandStart = starts[index];

        //Skips whitespaces, like command parsing does
        //TODO: factor it?
        while(commandText[commandStart] == ' ') {
            commandStart ++;
        }
        std::string command(commandText, starts[index], starts[index + 1] - starts[index]);

        //Split the command and find the arg to complete
        Cmd::Args args(command);
        int pos = cursor - starts[index];
        int argNum = args.PosToArg(pos);
        int startPos = args.ArgStartPos(argNum);

        std::vector<std::string> candidates = Cmd::CompleteArgument(command, pos);

        //Insert the longest common prefix of all the results
        if (candidates.size() > 0) {
            int prefixSize = candidates[0].size();
            for (auto candidate : candidates) {
                prefixSize = std::min(prefixSize, Str::LongestPrefixSize(candidate, candidates[0]));
            }

            std::string completedArg(candidates[0], 0, prefixSize);
            commandText.replace(commandStart + startPos, pos - startPos, completedArg);

            cursor = startPos + prefixSize + slashOffset;
        }

        //Print the matches if it is ambiguous
        //TODO: multi column nice print?
        if (candidates.size() >= 2) {
            for (auto candidate : candidates) {
                Com_Printf("  %s\n", candidate.c_str());
            }
        }

        UpdateScroll();
        Q_strncpyz(buffer + slashOffset, commandText.c_str(), bufferSize - 1);
    }

}
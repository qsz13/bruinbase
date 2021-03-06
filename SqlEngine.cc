/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include "Bruinbase.h"
#include "SqlEngine.h"
#include "BTreeIndex.h"

using namespace std;

// external functions and variables for load file and sql command parsing 
extern FILE *sqlin;

int sqlparse(void);



RC SqlEngine::run(FILE *commandline) {
    fprintf(stdout, "Bruinbase> ");

    // set the command line input and start parsing user input
    sqlin = commandline;
    sqlparse();  // sqlparse() is defined in SqlParser.tab.c generated from
    // SqlParser.y by bison (bison is GNU equivalent of yacc)

    return 0;
}


RC SqlEngine::select(int attr, const string &table, const vector<SelCond> &conds) {

    PageFile pf;

    if(pf.open(table+".idx", 'r')<0) return selectWithoutIndex(attr, table, conds);


    int tempMin, tempMax;
    CombinedCond cCond;
    if(conds.size()<1){
        if(attr == 2 || attr == 3) return selectWithoutIndex(attr, table, conds);
        else return selectWithIndex(attr, table, cCond, conds);

    }


    for(int i = 0; i < conds.size(); i++) {
        if(conds[i].attr == 1) { //key
            cCond.hasKey = true;
            int condValue = atoi(conds[i].value);
            switch(conds[i].comp) {
                case SelCond::EQ:
                    if(cCond.hasEqual && condValue != cCond.exactKey) return 0;
                    cCond.hasEqual = true;
                    if(cCond.hasNEqual && condValue == cCond.exactKey) return 0;
                    if(cCond.hasRange && (condValue>cCond.rangeMax||condValue<cCond.rangeMin)) return 0;
                    cCond.exactKey = condValue;

                    break;

                case SelCond::NE:
                    cCond.hasNEqual = true;
                    if(cCond.hasEqual && condValue == cCond.exactKey) return 0;
                    cCond.exactKey = condValue;

                    break;

                case SelCond::GT:
                    condValue+=1;
                case SelCond::GE:
                    cCond.hasRange = true;
                    tempMin = condValue;
                    if(tempMin > cCond.rangeMax) return 0;
                    cCond.rangeMin = max(cCond.rangeMin, tempMin);
                    break;

                case SelCond::LT:
                    condValue--;
                case SelCond::LE:
                    cCond.hasRange = true;
                    tempMax = condValue;
                    if(tempMax < cCond.rangeMin) return 0;
                    cCond.rangeMax = min(cCond.rangeMax, tempMax);
                    break;

            }

        }
        else {
            cCond.hasValue = true;
        }
    }
    if(cCond.hasEqual && cCond.hasNEqual) cCond.hasNEqual = false;
    if(cCond.hasRange && cCond.hasEqual) {
        if(cCond.exactKey < cCond.rangeMin || cCond.exactKey>cCond.rangeMax) return 0;
    }
    if((cCond.hasValue && ! cCond.hasKey)||(cCond.hasNEqual && !cCond.hasEqual && !cCond.hasRange)) {
        return selectWithoutIndex(attr, table, conds);
    } else {
        return selectWithIndex(attr, table, cCond, conds);
    }

}


RC SqlEngine::selectWithIndex(int attr, const std::string &table, const CombinedCond& cCond, const vector<SelCond> &conds) {
    BTreeIndex bi;
    RecordFile rf;   // RecordFile containing the table
    RecordId rid;  // record cursor for table scanning
    int rc;
    if((rc=bi.open(table+".idx", 'r'))<0) {
        return rc;
    }
    if(attr == 2 || attr == 3){
        if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
            fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
            return rc;
        }
    }
    int key;
    IndexCursor indexCursor;
    if(cCond.hasEqual){
        //search with cCond.exactKey
        key = cCond.exactKey;
        if((rc = bi.locate(key, indexCursor)) < 0 ) {
            return rc;
        } else {
            if(attr == 4){
                fprintf(stdout, "1\n");
            } else {
                bi.readForward(indexCursor,key,rid);
                printResult(attr, key, rid, rf);
            }
        }
    } else {
        //search starting from rangeMin
        int count = 0;
        key = cCond.rangeMin;
        bi.locate(key, indexCursor);
        while(bi.readForward(indexCursor, key, rid) == 0 && key <= cCond.rangeMax) {
            if(cCond.hasValue) {
                string value;
                for(int i = 0; i < conds.size(); i++) {
                    rf.read(rid,key, value);
                    if(conds[i].attr == 1) {
                        if(conds[i].comp == SelCond::EQ && conds[i].value!=value) {
                            break;
                        } else if(conds[i].comp == SelCond::NE && conds[i].value==value) {
                            break;
                        } else if(conds[i].comp == SelCond::GT && conds[i].value>=value) {
                            break;
                        } else if(conds[i].comp == SelCond::LT && conds[i].value<=value) {
                            break;
                        } else if(conds[i].comp == SelCond::GE && conds[i].value>value) {
                            break;
                        } else if(conds[i].comp == SelCond::LE && conds[i].value<value) {
                            break;
                        }
                    }
                    if (attr == 4) count++;
                    else printResult(attr, key, rid, rf, value);

                }

            }
            else {
                if (cCond.hasNEqual && key == cCond.exactKey) {
                    continue;
                }
                if (attr == 4) count++;
                else printResult(attr, key, rid, rf);
            }
        }
        if(attr == 4){
            fprintf(stdout, "%d\n", count);
        }
    }
    return 0;
}

RC SqlEngine::printResult(int attr, int key, RecordId& rid, RecordFile& rf) {
    string value;
    switch(attr) {
        case 1:
            fprintf(stdout, "%d\n", key);
            break;

        case 2:

            rf.read(rid,key, value);
            fprintf(stdout, "%s\n", value.c_str());
            break;
        case 3:
            rf.read(rid,key, value);
            fprintf(stdout, "%d '%s'\n", key, value.c_str());
            break;
    }
    return 0;
}

RC SqlEngine::printResult(int attr, int key, RecordId& rid, RecordFile& rf, string value) {
    switch(attr) {
        case 1:
            fprintf(stdout, "%d\n", key);
            break;

        case 2:
            fprintf(stdout, "%s\n", value.c_str());
            break;
        case 3:
            fprintf(stdout, "%d '%s'\n", key, value.c_str());
            break;
    }
    return 0;
}


RC SqlEngine::selectWithoutIndex(int attr, const std::string &table, const std::vector<SelCond> &cond) {
    RecordFile rf;   // RecordFile containing the table
    RecordId rid;  // record cursor for table scanning

    RC rc;
    int key;
    string value;
    int count;
    int diff;

    // open the table file
    if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
        fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
        return rc;
    }

    // scan the table file from the beginning
    rid.pid = rid.sid = 0;
    count = 0;
    while (rid < rf.endRid()) {
        // read the tuple
        if ((rc = rf.read(rid, key, value)) < 0) {
            fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
            goto exit_select;
        }

        // check the conditions on the tuple
        for (unsigned i = 0; i < cond.size(); i++) {
            // compute the difference between the tuple value and the condition value
            switch (cond[i].attr) {
                case 1:
                    diff = key - atoi(cond[i].value);
                    break;
                case 2:
                    diff = strcmp(value.c_str(), cond[i].value);
                    break;
            }

            // skip the tuple if any condition is not met
            switch (cond[i].comp) {
                case SelCond::EQ:
                    if (diff != 0) goto next_tuple;
                    break;
                case SelCond::NE:
                    if (diff == 0) goto next_tuple;
                    break;
                case SelCond::GT:
                    if (diff <= 0) goto next_tuple;
                    break;
                case SelCond::LT:
                    if (diff >= 0) goto next_tuple;
                    break;
                case SelCond::GE:
                    if (diff < 0) goto next_tuple;
                    break;
                case SelCond::LE:
                    if (diff > 0) goto next_tuple;
                    break;
            }
        }

        // the condition is met for the tuple.
        // increase matching tuple counter
        count++;

        // print the tuple
        switch (attr) {
            case 1:  // SELECT key
                fprintf(stdout, "%d\n", key);
                break;
            case 2:  // SELECT value
                fprintf(stdout, "%s\n", value.c_str());
                break;
            case 3:  // SELECT *
                fprintf(stdout, "%d '%s'\n", key, value.c_str());
                break;
        }

        // move to the next tuple
        next_tuple:
        ++rid;
    }

    // print matching tuple count if "select count(*)"
    if (attr == 4) {
        fprintf(stdout, "%d\n", count);
    }
    rc = 0;

    // close the table file and return
    exit_select:
    rf.close();
    return rc;
}




RC SqlEngine::load(const string &table, const string &loadfile, bool index) {
    /* your code here */
    string line;
    RC rc;
    RecordFile rf;
    BTreeIndex bi;

    if ((rc = rf.open(table + ".tbl", 'w')) < 0) {
        fprintf(stderr, "Error: open table %s failed\n", table.c_str());
        return rc;
    }

    if (index) {
        if ((rc = bi.open(table+".idx", 'w')) < 0) {
            fprintf(stderr, "Error: create index %s failed\n", table.c_str());
            rf.close();
            return rc;
        }

    }

    ifstream lfstream(loadfile.c_str());
    if (lfstream.is_open()) {
        while (getline(lfstream, line)) {
            int key;
            string value;
            if ((rc = parseLoadLine(line, key, value)) < 0) {
                fprintf(stderr, "Error: while parsing a line from file %s\n", loadfile.c_str());
                lfstream.close();
                rf.close();
                return rc;
            }
            RecordId rid;
            rf.append(key, value, rid);
            if (index) {
                bi.insert(key, rid);
            }
        }
    }

    if (index) bi.close();
    rf.close();
    lfstream.close();
    return 0;
}

RC SqlEngine::parseLoadLine(const string &line, int &key, string &value) {
    const char *s;
    char c;
    string::size_type loc;

    // ignore beginning white spaces
    c = *(s = line.c_str());
    while (c == ' ' || c == '\t') { c = *++s; }

    // get the integer key value
    key = atoi(s);

    // look for comma
    s = strchr(s, ',');
    if (s == NULL) { return RC_INVALID_FILE_FORMAT; }

    // ignore white spaces
    do { c = *++s; } while (c == ' ' || c == '\t');

    // if there is nothing left, set the value to empty string
    if (c == 0) {
        value.erase();
        return 0;
    }

    // is the value field delimited by ' or "?
    if (c == '\'' || c == '"') {
        s++;
    } else {
        c = '\n';
    }

    // get the value string
    value.assign(s);
    loc = value.find(c, 0);
    if (loc != string::npos) { value.erase(loc); }

    return 0;
}

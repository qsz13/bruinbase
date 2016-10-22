/*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <iostream>
#include <vector>
#include "BTreeIndex.h"

using namespace std;

/**
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex() {
    rootPid = -1;
    treeHeight = 0;
}

/**
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string &indexname, char mode) {
    int rc = 0;
    if ((rc = pf.open(indexname, mode)) < 0) {
        return rc;
    }
    if (pf.endPid() == 0) {
        // new index file
        writeBTreeMeta();
    } else {
        // not new index file
        readBTreeMeta();

    }
    return rc;
}

RC BTreeIndex::readBTreeMeta() {


    char metaPage[PageFile::PAGE_SIZE];
    int rc = pf.read(0, metaPage);
    rootPid = ((int *) metaPage)[0];
    treeHeight = ((int *) metaPage)[1];
    if (INFO) {
        cout << "loading: rootPid " << rootPid << endl;
        cout << "loading: read treeHeight " << treeHeight << endl;
    }
    return rc;
}

/**
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close() {
    return pf.close();
}

RC BTreeIndex::createNonLeafRoot(BTNonLeafNode &root, PageId pid1, int key, PageId pid2) {
    root.initializeRoot(pid1, key, pid2);
    writeBTreeMeta(root.getPageId(), treeHeight + 1);
    return 0;
}


RC BTreeIndex::writeBTreeMeta() {
    if (INFO) {
        cout << "writing: rootPid " << rootPid << endl;
        cout << "writing: treeHeight " << treeHeight << endl;
    }
    char metaPage[PageFile::PAGE_SIZE];
    ((int *) metaPage)[0] = rootPid;
    ((int *) metaPage)[1] = treeHeight;
    return pf.write(0, metaPage);
}

RC BTreeIndex::writeBTreeMeta(PageId rootPid, int treeHeight) {
    this->rootPid = rootPid;
    this->treeHeight = treeHeight;
    return writeBTreeMeta();
}

RC BTreeIndex::getLeafNodeToInsert(int key, PageId &pid, vector<PageId> &path) {
    pid = rootPid;
    int level = 1;
    while (level++ < treeHeight) {
        BTNonLeafNode nonLeafNode(pid, pf);
        path.push_back(pid);
        nonLeafNode.locateChildPtr(key, pid);
    }
    return 0;
}

/**
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId &rid) {
    int rc = 0;
    vector<PageId> path;
    if (rootPid == -1) {             // Tree is empty
        BTLeafNode root(pf);
        root.insert(key, rid);
        writeBTreeMeta(root.getPageId(), 1);
    } else {
        PageId pid;
        getLeafNodeToInsert(key, pid, path);

        BTLeafNode leafToInsert(pid, pf);
        if ((rc = leafToInsert.insert(key, rid)) == RC_NODE_FULL) {

            if (DEBUG) cout << "Leaf node " << leafToInsert.getPageId() << " is full!" << endl;

            BTLeafNode leafSib(pf);
            int leafSibKey;
            leafToInsert.insertAndSplit(key, rid, leafSib, leafSibKey);

            if (DEBUG) {
                cout << endl << "After split: " << endl;
                leafToInsert.printNode();
                cout << "-------------" << endl;
                leafSib.printNode();
            }

            PageId childPid = leafSib.getPageId();
            int childKey = leafSibKey;
            PageId parentID = leafToInsert.getPageId();
            while (!path.empty()) {
                parentID = path.back();
                path.pop_back();
                BTNonLeafNode parent(parentID, pf);
                if ((rc = parent.insert(childKey, childPid)) == 0) {
                    if (DEBUG) parent.printNode();
                    return rc;
                }
                BTNonLeafNode nonLeafSib(pf);
                int nonLeafSibKey;
                parent.insertAndSplit(childKey, childPid, nonLeafSib, nonLeafSibKey);

                if (DEBUG) {
                    cout << endl << "After split of " << parentID << ": " << endl;
                    cout << "------ Left -------" << endl;
                    parent.printNode();
                    cout << "------ Right -------" << endl;
                    nonLeafSib.printNode();
                    cout << "-------------------" << endl;

                }

                childKey = nonLeafSibKey;
                childPid = nonLeafSib.getPageId();
            }
            BTNonLeafNode newRoot(pf);
            createNonLeafRoot(newRoot, parentID, childKey, childPid);
            if (DEBUG) newRoot.printNode();

        }
        if (DEBUG) leafToInsert.printNode();
    }
    return rc;
}

/**
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code
 */
RC BTreeIndex::locate(int searchKey, IndexCursor &cursor) {
    int rc = 0;
    int pid = rootPid;
    if (rootPid <= 0)
        return RC_NO_SUCH_RECORD;
    for (int level = 1; level < treeHeight; level++) {
        BTNonLeafNode current(pid, pf);
        current.locateChildPtr(searchKey, pid);
    }
    BTLeafNode leaf(pid, pf);
    int eid = 0;
    rc = leaf.locate(searchKey, eid);
    cursor.eid = eid;
    cursor.pid = pid;
    return rc;
}

/**
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor &cursor, int &key, RecordId &rid) {
    if(cursor.pid < 0) return RC_END_OF_TREE;
    BTLeafNode leaf(cursor.pid, pf);
    key = leaf.getKeyByEid(cursor.eid);
    rid = leaf.getRidByEid(cursor.eid);
    return leaf.forward(cursor.pid, cursor.eid);
}



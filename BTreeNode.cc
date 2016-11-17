#include "BTreeNode.h"
#include "RecordFile.h"
#include <iostream>
#include <cstring>
#include <cstdio>

using namespace std;

BTreeNode::BTreeNode(PageFile &pf) : pageFile(pf), pageId(pf.endPid()) {}

BTreeNode::BTreeNode(PageId pid, PageFile &pf) : pageFile(pf), pageId(pid) {}


RC BTreeNode::binarySearch(int keys[], int low, int high, int target, int &idx) const {
    if (low >= high - 1) {
        if (keys[low] == target) {
            idx = low;
            return 0;
        } else if (keys[high] == target) {
            idx = high;
            return 0;
        } else if (keys[low] > target) {
            idx = low;
            return RC_NO_SUCH_RECORD;
        } else if (keys[high] > target) {
            idx = high;
            return RC_NO_SUCH_RECORD;
        } else {
            idx = high + 1;
            return RC_NO_SUCH_RECORD;
        }
    }
    int mid = (low + high) / 2;
    if (keys[mid] > target) return binarySearch(keys, low, mid, target, idx);
    else if (keys[mid] < target) return binarySearch(keys, mid, high, target, idx);
    else {
        idx = mid;
        return 0;
    }
}


/**
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTreeNode::read(PageId pid, const PageFile &pf) {
    int rc = 0;
    if ((rc = pf.read(pid, buffer)) < 0) {
        fprintf(stderr, "Error: read from Page failed\n");
        return rc;
    }
    pageId = pid;
    pageFile = pf;
    return 0;
}


/**
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTreeNode::write(PageId pid, PageFile &pf) {
    int rc = 0;

    if ((rc = pf.write(pid, buffer)) < 0) {
        fprintf(stderr, "Error: write to Page failed\n");
        return rc;
    }
    return 0;
}

RC BTreeNode::write() {
    return write(pageId, pageFile);
}

int BTreeNode::getPageId() const {
    return pageId;
}



//***********************************************************************

BTLeafNode::BTLeafNode(PageFile &pf) : BTreeNode(pf) {
    setKeyCount(0);
    setNextNodePtr(-1);
    write(pageId, pageFile);
}

BTLeafNode::BTLeafNode(PageId pid, PageFile &pf) : BTreeNode(pid, pf) {
    read(pid, pf);
}


/**
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount() const {
    return ((LeafNode *) buffer)->keyCount;
}

void BTLeafNode::setKeyCount(int count) {
    ((LeafNode *) buffer)->keyCount = count;
}

/**
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId &rid) {
    if (DEBUG)
        cout << "inserting key: " << key << " rid: " << rid.sid << " " << rid.pid << " into node " << getPageId()
             << ". " << endl;
    int keyCount = getKeyCount();
    if (isFull()) {
        return RC_NODE_FULL;
    }
    int *keys = getKeys();
    RecordId *rids = getRecords();
    int i = keyCount;
    for (; keys[i - 1] > key && i > 0; i--) {
        keys[i] = keys[i - 1];
        rids[i] = rids[i - 1];

    }
    keys[i] = key;
    rids[i] = rid;
    setKeyCount(keyCount + 1);
    if (DEBUG) cout << "KEYCOUNT:" << getKeyCount() << endl;
    write();
    return 0;
}

/**
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId &rid,
                              BTLeafNode &sibling, int &siblingKey) {
    int *keys = getKeys(), *siblingKeys = sibling.getKeys();
    RecordId *rids = getRecords(), *siblingRids = sibling.getRecords();
    int start = key < keys[BT_MAX_KEY / 2] ? BT_MAX_KEY / 2 : (BT_MAX_KEY + 1) / 2;
    memcpy(siblingKeys, keys + start, (BT_MAX_KEY - start) * sizeof(int));
    memcpy(siblingRids, rids + start, (BT_MAX_KEY - start) * sizeof(RecordId));
    setKeyCount(start);
    sibling.setKeyCount(BT_MAX_KEY - start);

    int *insertKeys = key > keys[start - 1] ? siblingKeys : keys;
    RecordId *insertRids = key > keys[start - 1] ? siblingRids : rids;
    int i = key > keys[start - 1] ? sibling.getKeyCount() : getKeyCount();
    for (; insertKeys[i - 1] > key && i > 0; i--) {
        insertKeys[i] = insertKeys[i - 1];
        insertRids[i] = insertRids[i - 1];
    }
    insertKeys[i] = key;
    insertRids[i] = rid;

    if (key > keys[start - 1]) sibling.setKeyCount(sibling.getKeyCount() + 1);
    else setKeyCount(getKeyCount() + 1);

    PageId temp = getNextNodePtr();
    setNextNodePtr(sibling.getPageId());
    sibling.setNextNodePtr(temp);
    siblingKey = siblingKeys[0];
    sibling.write();
    write();
    return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int &eid) {
    return binarySearch(getKeys(), 0, getKeyCount() - 1, searchKey, eid);
}

/**
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int &key, RecordId &rid) {
    RecordId *rids = getRecords();
    int *keys = getKeys();
    if (keys[eid] != key) return RC_NO_SUCH_RECORD;
    rid = rids[eid];
    return 0;
}

/**
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr() const {
    return ((LeafNode *) buffer)->nextPid;
}

/**
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid) {
    ((LeafNode *) buffer)->nextPid = pid;
    return 0;
}


int *BTLeafNode::getKeys() const {
    return ((LeafNode *) buffer)->keys;
}

RecordId *BTLeafNode::getRecords() const {
    return ((LeafNode *) buffer)->rids;
}

int BTLeafNode::getKeyByEid(int eid) const {
    return getKeys()[eid];
}
RecordId BTLeafNode::getRidByEid(int eid) const {
    return getRecords()[eid];
}

RC BTLeafNode::forward(PageId &pid, int& eid) {
    if(pid < 0) return RC_END_OF_TREE;
    if(++eid >=getKeyCount()) {
        pid = getNextNodePtr();
        eid = 0;
    }
    return 0;
}


void BTLeafNode::printNode() const {
    int keyCount = getKeyCount();
    int *keys = getKeys();
    RecordId *rids = getRecords();
    PageId next = getNextNodePtr();
    cout << endl << "#####################  Leaf Node " << getPageId() << " ########################" << endl;
    cout << "keyCount: " << keyCount << endl << "keys: ";
    for (int i = 0; i < keyCount; i++) {
        cout << keys[i] << " ";
    }
    cout << endl << "rids: " << endl << "pid : ";
    for (int i = 0; i < keyCount; i++) {
        cout << rids[i].pid << " ";
    }
    cout << endl << "sid : ";
    for (int i = 0; i < keyCount; i++) {
        cout << rids[i].sid << " ";
    }
    cout << endl;
    cout << "next: " << next << endl;
    cout << "##################################################" << endl << endl;
}

bool BTLeafNode::isFull() const {
    return getKeyCount() >= BT_MAX_KEY;
}


//*********************************************************************************************

BTNonLeafNode::BTNonLeafNode(PageFile &pf) : BTreeNode(pf) {
    setKeyCount(0);
    write(pageId, pageFile);
}

BTNonLeafNode::BTNonLeafNode(PageId pid, PageFile &pf) : BTreeNode(pid, pf) {
    read(pid, pf);
}


/**
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid) {
    if (isFull()) {
        return RC_NODE_FULL;
    }

    return forceInsert(key, pid);
}


RC BTNonLeafNode::forceInsert(int key, PageId pid) {
    int keyCount = getKeyCount();
    int *keys = getKeys();
    PageId *pids = getPages();
    int i = keyCount;
    for (; keys[i - 1] > key && i > 0; i--) {
        keys[i] = keys[i - 1];
        pids[i + 1] = pids[i];
    }
    keys[i] = key;
    pids[i + 1] = pid;
    setKeyCount(keyCount + 1);
    return write();
}


/**
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode &sibling, int &midKey) {
    // TODO different from leaf node
    int *keys = getKeys(), *siblingKeys = sibling.getKeys();
    PageId *pids = getPages(), *siblingPids = sibling.getPages();

    forceInsert(key, pid);
    int size = BT_MAX_KEY + 1;
    midKey = keys[size / 2];
    int i = size / 2 + 1, j = 0;
    for (; i < size; i++, j++) {
        siblingKeys[j] = keys[i];
        siblingPids[j] = pids[i];
    }
    siblingPids[j] = pids[i];

    setKeyCount((BT_MAX_KEY + 1) / 2);
    sibling.setKeyCount(BT_MAX_KEY / 2);
    sibling.write();
    write();
    return 0;

}

/**
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId &pid) {
    int index = -1;
    if(binarySearch(getKeys(), 0, getKeyCount() - 1, searchKey, index)==0) index++;
    pid = getPages()[index];
    return 0;
}

/**
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2) {
    PageId *pids = getPages();
    pids[0] = pid1;
    pids[1] = pid2;
    int *keys = getKeys();
    keys[0] = key;
    setKeyCount(1);
    write();
    if (DEBUG) {
        cout << "Initialize Root " << getPageId() << endl;
    }
    return 0;
}

/**
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount() const {
    return ((NonLeafNode *) buffer)->keyCount;
}

void BTNonLeafNode::setKeyCount(int count) {
    ((NonLeafNode *) buffer)->keyCount = count;
}

int *BTNonLeafNode::getKeys() const {
    return ((NonLeafNode *) buffer)->keys;
}

PageId *BTNonLeafNode::getPages() const {
    return ((NonLeafNode *) buffer)->pids;
}


bool BTNonLeafNode::isFull() const {
    return getKeyCount() >= BT_MAX_KEY;
}

int BTNonLeafNode::getPidCount() const {
    return getKeyCount() + 1;
}


void BTNonLeafNode::printNode() const {
    int keyCount = getKeyCount();
    int *keys = getKeys();
    PageId *pids = getPages();
    cout << endl << "####################  Non Leaf Node " << getPageId() << " #########################" << endl;
    cout << "keyCount: " << keyCount << endl << "keys: ";
    for (int i = 0; i < keyCount; i++) {
        cout << keys[i] << " ";
    }
    cout << endl << "pids: ";
    for (int i = 0; i < keyCount + 1; i++) {
        cout << pids[i] << " ";
    }
    cout << endl;
    cout << "##################################################" << endl << endl;

}
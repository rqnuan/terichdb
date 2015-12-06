/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo_narkdb_common.hpp"

#include "mongo/base/status_with.h"
#include "mongo/db/storage/index_entry_comparison.h"
#include "mongo/db/storage/sorted_data_interface.h"
#include "mongo/db/index/index_descriptor.h"
#include "narkdb_recovery_unit.h"

namespace mongo { namespace narkdb {

class NarkDbIndex : public SortedDataInterface {
public:
    /**
     * Parses index options for wired tiger configuration string suitable for table creation.
     * The document 'options' is typically obtained from the 'storageEngine.narkDb' field
     * of an IndexDescriptor's info object.
     */
    static StatusWith<std::string> parseIndexOptions(const BSONObj& options);

    /**
     * Creates a configuration string suitable for 'config' parameter in NarkDb_SESSION::create().
     * Configuration string is constructed from:
     *     built-in defaults
     *     'sysIndexConfig'
     *     'collIndexConfig'
     *     storageEngine.narkDb.configString in index descriptor's info object.
     * Performs simple validation on the supplied parameters.
     * Returns error status if validation fails.
     * Note that even if this function returns an OK status, NarkDb_SESSION:create() may still
     * fail with the constructed configuration string.
     */
    static StatusWith<std::string> generateCreateString(const std::string& engineName,
                                                        const std::string& sysIndexConfig,
                                                        const std::string& collIndexConfig,
                                                        const IndexDescriptor& desc);

    /**
     * @param unique - If this is a unique index.
     *                 Note: even if unique, it may be allowed to be non-unique at times.
     */
    NarkDbIndex(CompositeTable* table, OperationContext* ctx, const IndexDescriptor* desc);

    virtual Status insert(OperationContext* txn,
                          const BSONObj& key,
                          const RecordId& id,
                          bool dupsAllowed);

    virtual void unindex(OperationContext* txn,
                         const BSONObj& key,
                         const RecordId& id,
                         bool dupsAllowed);

    virtual void fullValidate(OperationContext* txn,
                              bool full,
                              long long* numKeysOut,
                              BSONObjBuilder* output) const;
    virtual bool appendCustomStats(OperationContext* txn,
                                   BSONObjBuilder* output,
                                   double scale) const;
    virtual Status dupKeyCheck(OperationContext* txn,
							   const BSONObj& key, const RecordId& id);

    virtual bool isEmpty(OperationContext* txn);

    virtual Status touch(OperationContext* txn) const;

    virtual long long getSpaceUsedBytes(OperationContext* txn) const;

    virtual Status initAsEmpty(OperationContext* txn);

    const std::string& uri() const {
        return _uri;
    }

    Ordering ordering() const {
        return _ordering;
    }

    virtual bool unique() const = 0;

    Status dupKeyError(const BSONObj& key);

    // nark::db
    size_t m_indexId;
    nark::db::CompositeTablePtr m_table;

	const nark::db::Schema* getIndexSchema() const {
		return &m_table->getIndexSchema(m_indexId);
	}

	bool insertIndexKey(const BSONObj& newKey, const RecordId& id,
						nark::db::DbContext*);

protected:
    class BulkBuilder;
	class MyThreadData {
    public:
    	nark::db::DbContextPtr m_dbCtx;
    	mongo::narkdb::SchemaRecordCoder m_coder;
    	nark::valvec<unsigned char> m_buf;
    };
	mutable gold_hash_map<std::thread::id, MyThreadData> m_threadcache;
	mutable std::mutex m_threadcacheMutex;
    MyThreadData& getMyThreadData() const;

    const Ordering _ordering;
    std::string _uri;
    std::string _collectionNamespace;
    std::string _indexName;
};


class NarkDbIndexUnique : public NarkDbIndex {
public:
    NarkDbIndexUnique(CompositeTable* tab,
                      OperationContext* opCtx,
                      const IndexDescriptor* desc);

    std::unique_ptr<SortedDataInterface::Cursor>
    newCursor(OperationContext* txn, bool forward) const override;

    SortedDataBuilderInterface*
	getBulkBuilder(OperationContext* txn, bool dupsAllowed) override;

    bool unique() const override { return true; }
};

class NarkDbIndexStandard : public NarkDbIndex {
public:
    NarkDbIndexStandard(CompositeTable* tab,
                        OperationContext* opCtx,
                        const IndexDescriptor* desc);

    std::unique_ptr<SortedDataInterface::Cursor>
    newCursor(OperationContext* txn, bool forward) const override;

    SortedDataBuilderInterface*
	getBulkBuilder(OperationContext* txn, bool dupsAllowed) override;

    bool unique() const override { return false; }
};

} }  // namespace mongo::nark


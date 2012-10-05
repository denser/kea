// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <auth/datasrc_config.h>

#include <config/tests/fake_session.h>
#include <config/ccsession.h>
#include <util/threads/lock.h>

#include <gtest/gtest.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <memory>

using namespace isc;
using namespace isc::cc;
using namespace isc::config;
using namespace isc::data;
using namespace isc::dns;
using namespace std;
using namespace boost;

namespace {

class DatasrcConfigTest;

class FakeList {
public:
    FakeList(const RRClass&) :
        configuration_(new ListElement)
    {}
    void configure(const ConstElementPtr& configuration, bool allow_cache) {
        EXPECT_TRUE(allow_cache);
        conf_ = configuration->get(0)->get("type")->stringValue();
        configuration_ = configuration;
    }
    const string& getConf() const {
        return (conf_);
    }
    ConstElementPtr getConfiguration() const {
        return (configuration_);
    }
private:
    string conf_;
    ConstElementPtr configuration_;
};

typedef shared_ptr<FakeList> ListPtr;

void
testConfigureDataSource(DatasrcConfigTest& test,
                        const isc::data::ConstElementPtr& config)
{
    // We use the test fixture for the Server type.  This makes it possible
    // to easily fake all needed methods and look that they were called.
    configureDataSourceGeneric<DatasrcConfigTest, FakeList>(test, config);
}

void
datasrcConfigHandler(DatasrcConfigTest* fake_server, const std::string&,
                     isc::data::ConstElementPtr config,
                     const isc::config::ConfigData&)
{
    if (config->contains("classes")) {
        testConfigureDataSource(*fake_server, config->get("classes"));
    }
}

class DatasrcConfigTest : public ::testing::Test {
public:
    // To pretend to be the server:
    void swapDataSrcClientLists(shared_ptr<std::map<dns::RRClass, ListPtr> >
                                new_lists)
    {
        lists_.clear();         // first empty it

        // Record the operation and results.  Note that map elements are
        // sorted by RRClass, so the ordering should be predictable.
        for (std::map<dns::RRClass, ListPtr>::const_iterator it =
                 new_lists->begin();
             it != new_lists->end();
             ++it)
        {
            const RRClass rrclass = it->first;
            ListPtr list = it->second;
            log_ += "set " + rrclass.toText() + " " +
                (list ? list->getConf() : "") + "\n";
            lists_[rrclass] = list;
        }
    }

protected:
    DatasrcConfigTest() :
        session(ElementPtr(new ListElement), ElementPtr(new ListElement),
                ElementPtr(new ListElement)),
        specfile(string(TEST_OWN_DATA_DIR) + "/spec.spec")
    {
        initSession();
    }
    void initSession() {
        session.getMessages()->add(createAnswer());
        mccs.reset(new ModuleCCSession(specfile, session, NULL, NULL, false,
                                       false));
    }
    void TearDown() {
        // Make sure no matter what we did, it is cleaned up.  Also check
        // we really have subscribed to the configuration, and after removing
        // it we actually cancel it.
        EXPECT_TRUE(session.haveSubscription("data_sources", "*"));
        mccs->removeRemoteConfig("data_sources");
        EXPECT_FALSE(session.haveSubscription("data_sources", "*"));
    }
    void SetUp() {
        session.getMessages()->
            add(createAnswer(0,
                             moduleSpecFromFile(string(PLUGIN_DATA_PATH) +
                                                "/datasrc.spec").
                             getFullSpec()));
        session.getMessages()->add(createAnswer(0,
                                                ElementPtr(new MapElement)));
        mccs->addRemoteConfig("data_sources",
                              boost::bind(datasrcConfigHandler,
                                          this, _1, _2, _3), false);
    }
    ElementPtr buildConfig(const string& config) const {
        const ElementPtr internal(Element::fromJSON(config));
        const ElementPtr external(Element::fromJSON("{\"version\": 1}"));
        external->set("classes", internal);
        return (external);
    }
    void initializeINList() {
        const ConstElementPtr
            config(buildConfig("{\"IN\": [{\"type\": \"xxx\"}]}"));
        session.addMessage(createCommand("config_update", config),
                           "data_sources", "*");
        mccs->checkCommand();
        // Check that the passed config is stored.
        EXPECT_EQ("set IN xxx\n", log_);
        EXPECT_EQ(1, lists_.size());
    }
    FakeSession session;
    auto_ptr<ModuleCCSession> mccs;
    const string specfile;
    std::map<RRClass, ListPtr> lists_;
    string log_;
    mutable isc::util::thread::Mutex mutex_;
};

// Push there a configuration with a single list.
TEST_F(DatasrcConfigTest, createList) {
    initializeINList();
}

TEST_F(DatasrcConfigTest, modifyList) {
    // First, initialize the list, and confirm the current config
    initializeINList();
    EXPECT_EQ("xxx", lists_[RRClass::IN()]->getConf());

    // And now change the configuration of the list
    const ElementPtr
        config(buildConfig("{\"IN\": [{\"type\": \"yyy\"}]}"));
    session.addMessage(createCommand("config_update", config), "data_sources",
                       "*");
    log_ = "";
    mccs->checkCommand();
    // Now the new one should be installed.
    EXPECT_EQ("yyy", lists_[RRClass::IN()]->getConf());
    EXPECT_EQ(1, lists_.size());
}

// Check we can have multiple lists at once
TEST_F(DatasrcConfigTest, multiple) {
    const ElementPtr
        config(buildConfig("{\"IN\": [{\"type\": \"yyy\"}], "
                                 "\"CH\": [{\"type\": \"xxx\"}]}"));
    session.addMessage(createCommand("config_update", config), "data_sources",
                       "*");
    mccs->checkCommand();
    // We have set commands for both classes.
    EXPECT_EQ("set IN yyy\nset CH xxx\n", log_);
    // We should have both there
    EXPECT_EQ("yyy", lists_[RRClass::IN()]->getConf());
    EXPECT_EQ("xxx", lists_[RRClass::CH()]->getConf());
    EXPECT_EQ(2, lists_.size());
}

// Check we can add another one later and the old one does not get
// overwritten.
//
// It's almost like above, but we initialize first with single-list
// config.
TEST_F(DatasrcConfigTest, updateAdd) {
    initializeINList();
    const ElementPtr
        config(buildConfig("{\"IN\": [{\"type\": \"yyy\"}], "
                           "\"CH\": [{\"type\": \"xxx\"}]}"));
    session.addMessage(createCommand("config_update", config), "data_sources",
                       "*");
    log_ = "";
    mccs->checkCommand();
    EXPECT_EQ("set IN yyy\nset CH xxx\n", log_);
    EXPECT_EQ("xxx", lists_[RRClass::CH()]->getConf());
    EXPECT_EQ("yyy", lists_[RRClass::IN()]->getConf());
    EXPECT_EQ(2, lists_.size());
}

// We delete a class list in this test.
TEST_F(DatasrcConfigTest, updateDelete) {
    initializeINList();
    const ElementPtr
        config(buildConfig("{}"));
    session.addMessage(createCommand("config_update", config), "data_sources",
                       "*");
    log_ = "";
    mccs->checkCommand();

    // No operation takes place in the configuration, and the old one is
    // just dropped
    EXPECT_EQ("", log_);
    EXPECT_TRUE(lists_.empty());
}

// Check that we can rollback an addition if something else fails
TEST_F(DatasrcConfigTest, rollbackAddition) {
    initializeINList();
    // The configuration is wrong. However, the CH one will get done first.
    const ElementPtr
        config(buildConfig("{\"IN\": [{\"type\": 13}], "
                           "\"CH\": [{\"type\": \"xxx\"}]}"));
    session.addMessage(createCommand("config_update", config), "data_sources",
                       "*");
    log_ = "";
    // It does not throw, as it is handled in the ModuleCCSession.
    // Throwing from the reconfigure is checked in other tests.
    EXPECT_NO_THROW(mccs->checkCommand());
    // Anyway, the result should not contain CH now and the original IN should
    // be there.
    EXPECT_EQ("xxx", lists_[RRClass::IN()]->getConf());
    EXPECT_FALSE(lists_[RRClass::CH()]);
}

// Check that we can rollback a deletion if something else fails
TEST_F(DatasrcConfigTest, rollbackDeletion) {
    initializeINList();
    // Put the CH there
    const ElementPtr
        config1(Element::fromJSON("{\"IN\": [{\"type\": \"yyy\"}], "
                                  "\"CH\": [{\"type\": \"xxx\"}]}"));
    testConfigureDataSource(*this, config1);
    const ElementPtr
        config2(Element::fromJSON("{\"IN\": [{\"type\": 13}]}"));
    // This would delete CH. However, the IN one fails.
    // As the deletions happen after the additions/settings
    // and there's no known way to cause an exception during the
    // deletions, it is not a true rollback, but the result should
    // be the same.
    EXPECT_THROW(testConfigureDataSource(*this, config2), TypeError);
    EXPECT_EQ("yyy", lists_[RRClass::IN()]->getConf());
    EXPECT_EQ("xxx", lists_[RRClass::CH()]->getConf());
}

// Check that we can roll back configuration change if something
// fails later on.
TEST_F(DatasrcConfigTest, rollbackConfiguration) {
    initializeINList();
    // Put the CH there
    const ElementPtr
        config1(Element::fromJSON("{\"IN\": [{\"type\": \"yyy\"}], "
                                  "\"CH\": [{\"type\": \"xxx\"}]}"));
    testConfigureDataSource(*this, config1);
    // Now, the CH happens first. But nevertheless, it should be
    // restored to the previoeus version.
    const ElementPtr
        config2(Element::fromJSON("{\"IN\": [{\"type\": 13}], "
                                  "\"CH\": [{\"type\": \"yyy\"}]}"));
    EXPECT_THROW(testConfigureDataSource(*this, config2), TypeError);
    EXPECT_EQ("yyy", lists_[RRClass::IN()]->getConf());
    EXPECT_EQ("xxx", lists_[RRClass::CH()]->getConf());
}

}

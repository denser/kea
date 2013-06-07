// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef D_TEST_STUBS_H
#define D_TEST_STUBS_H

#include <asiolink/asiolink.h>
#include <cc/data.h>
#include <cc/session.h>
#include <config/ccsession.h>

#include <d2/d_controller.h>
#include <gtest/gtest.h>

namespace isc {
namespace d2 {

/// @brief Class is used to set a globally accessible value that indicates
/// a specific type of failure to simulate.  Test derivations of base classes
/// can exercise error handling code paths by testing for specific SimFailure
/// values at the appropriate places and then causing the error to "occur".
/// The class consists of an enumerated set of failures, and static methods
/// for getting, setting, and testing the current value.
class SimFailure {
public:
    enum FailureType {
        ftUnknown = -1,
        ftNoFailure = 0,
        ftCreateProcessException,
        ftCreateProcessNull,
        ftProcessInit,
        ftProcessConfigure,
        ftControllerCommand,
        ftProcessCommand,
        ftProcessShutdown
    };

    /// @brief Sets the SimFailure value to the given value.
    ///
    /// @param value is the new value to assign to the global value.
    static void set(enum FailureType value) {
       failure_type_ = value;
    }

    /// @brief Gets the current global SimFailure value
    ///
    /// @return returns the current SimFailure value
    static enum FailureType get() {
       return (failure_type_);
    }

    /// @brief One-shot test of the SimFailure value. If the global
    /// SimFailure value is equal to the given value, clear the global
    /// value and return true.  This makes it convenient for code to
    /// test and react without having to explicitly clear the global
    /// value.
    ///
    /// @param value is the value against which the global value is
    /// to be compared.
    ///
    /// @return returns true if current SimFailure value matches the
    /// given value.
    static bool shouldFailOn(enum FailureType value) {
        if (failure_type_ == value) {
            clear();
            return (true);
        }

        return (false);
    }

    static void clear() {
       failure_type_ = ftNoFailure;
    }

    static enum FailureType failure_type_;
};

/// @brief Test Derivation of the DProcessBase class.
///
/// This class is used primarily to server as a test process class for testing
/// DControllerBase.  It provides minimal, but sufficient implementation to
/// test the majority of DControllerBase functionality.
class DStubProcess : public DProcessBase {
public:

    /// @brief Static constant that defines a custom process command string.
    static const char* stub_proc_command_;

    /// @brief Constructor
    ///
    /// @param name name is a text label for the process. Generally used
    /// in log statements, but otherwise arbitrary.
    /// @param io_service is the io_service used by the caller for
    /// asynchronous event handling.
    ///
    /// @throw DProcessBaseError is io_service is NULL.
    DStubProcess(const char* name, IOServicePtr io_service);

    /// @brief Invoked after process instantiation to perform initialization.
    /// This implementation supports simulating an error initializing the
    /// process by throwing a DProcessBaseError if SimFailure is set to
    /// ftProcessInit.
    virtual void init();

    /// @brief Implements the process's event loop.
    /// This implementation is quite basic, surrounding calls to
    /// io_service->runOne() with a test of the shutdown flag. Once invoked,
    /// the method will continue until the process itself is exiting due to a
    /// request to shutdown or some anomaly forces an exit.
    /// @return  returns 0 upon a successful, "normal" termination, non-zero to
    /// indicate an abnormal termination.
    virtual void run();

    /// @brief Implements the process shutdown procedure. Currently this is
    /// limited to setting the instance shutdown flag, which is monitored in
    /// run().
    virtual void shutdown();

    /// @brief Processes the given configuration.
    ///
    /// This implementation fails if SimFailure is set to ftProcessConfigure.
    /// Otherwise it will complete successfully.  It does not check the content
    /// of the inbound configuration.
    ///
    /// @param config_set a new configuration (JSON) for the process
    /// @return an Element that contains the results of configuration composed
    /// of an integer status value (0 means successful, non-zero means failure),
    /// and a string explanation of the outcome.
    virtual isc::data::ConstElementPtr configure(isc::data::ConstElementPtr
                                                 config_set);

    /// @brief Executes the given command.
    ///
    /// This implementation will recognizes one "custom" process command,
    /// stub_proc_command_.  It will fail if SimFailure is set to
    /// ftProcessCommand.
    ///
    /// @param command is a string label representing the command to execute.
    /// @param args is a set of arguments (if any) required for the given
    /// command.
    /// @return an Element that contains the results of command composed
    /// of an integer status value and a string explanation of the outcome.
    /// The status value is:
    /// COMMAND_SUCCESS if the command is recognized and executes successfully.
    /// COMMAND_ERROR if the command is recognized but fails to execute.
    /// COMMAND_INVALID if the command is not recognized.
    virtual isc::data::ConstElementPtr command(const std::string& command,
                                               isc::data::ConstElementPtr args);

    // @brief Destructor
    virtual ~DStubProcess();
};


/// @brief Test Derivation of the DControllerBase class.
///
/// DControllerBase is an abstract class and therefore requires a derivation
/// for testing.  It allows testing the majority of the base class code
/// without polluting production derivations (e.g. D2Process).  It uses
/// DStubProcess as its application process class.  It is a full enough
/// implementation to support running both stand alone and integrated.
/// Obviously BIND10 connectivity is not available under unit tests, so
/// testing here is limited to "failures" to communicate with BIND10.
class DStubController : public DControllerBase {
public:
    /// @brief Static singleton instance method. This method returns the
    /// base class singleton instance member.  It instantiates the singleton
    /// and sets the base class instance member upon first invocation.
    ///
    /// @return returns a pointer reference to the singleton instance.
    static DControllerBasePtr& instance();

    /// @brief Defines a custom controller command string. This is a
    /// custom command supported by DStubController.
    static const char* stub_ctl_command_;

    /// @brief Defines a custom command line option supported by
    /// DStubController.
    static const char* stub_option_x_;

protected:
    /// @brief Handles additional command line options that are supported
    /// by DStubController.  This implementation supports an option "-x".
    ///
    /// @param option is the option "character" from the command line, without
    /// any prefixing hyphen(s)
    /// @optarg optarg is the argument value (if one) associated with the option
    ///
    /// @return returns true if the option is "x", otherwise ti returns false.
    virtual bool customOption(int option, char *optarg);

    /// @brief Instantiates an instance of DStubProcess.
    ///
    /// This implementation will fail if SimFailure is set to
    /// ftCreateProcessException OR ftCreateProcessNull.
    ///
    /// @return returns a pointer to the new process instance (DProcessBase*)
    /// or NULL if SimFailure is set to ftCreateProcessNull.
    /// @throw throws std::runtime_error if SimFailure is set to
    /// ftCreateProcessException.
    virtual DProcessBase* createProcess();

    /// @brief Executes custom controller commands are supported by
    /// DStubController. This implementation supports one custom controller
    /// command, stub_ctl_command_.  It will fail if SimFailure is set
    /// to ftControllerCommand.
    ///
    /// @param command is a string label representing the command to execute.
    /// @param args is a set of arguments (if any) required for the given
    /// command.
    /// @return an Element that contains the results of command composed
    /// of an integer status value and a string explanation of the outcome.
    /// The status value is:
    /// COMMAND_SUCCESS if the command is recognized and executes successfully.
    /// COMMAND_ERROR if the command is recognized but fails to execute.
    /// COMMAND_INVALID if the command is not recognized.
    virtual isc::data::ConstElementPtr customControllerCommand(
            const std::string& command, isc::data::ConstElementPtr args);

    /// @brief Provides a string of the additional command line options
    /// supported by DStubController.  DStubController supports one
    /// addition option, stub_option_x_.
    ///
    /// @return returns a string containing the option letters.
    virtual const std::string getCustomOpts() const;

private:
    /// @brief Constructor is private to protect singleton integrity.
    DStubController();

public:
    virtual ~DStubController();
};

/// @brief Abstract Test fixture class that wraps a DControllerBase. This class
/// is a friend class of DControllerBase which allows it access to class
/// content to facilitate testing.  It provides numerous wrapper methods for
/// the protected and private methods and member of the base class.
class DControllerTest : public ::testing::Test {
public:

    /// @brief Defines a function pointer for controller singleton fetchers.
    typedef DControllerBasePtr& (*InstanceGetter)();

    /// @brief Static storage of the controller class's singleton fetcher.
    /// We need this this statically available for callbacks.
    static InstanceGetter instanceGetter_;

    /// @brief Constructor
    ///
    /// @param instance_getter is a function pointer to the static instance
    /// method of the DControllerBase derivation under test.
    DControllerTest(InstanceGetter instance_getter) {
        // Set the static fetcher member, then invoke it via getController.
        // This ensures the singleton is instantiated.
        instanceGetter_ = instance_getter;
        getController();
    }

    /// @brief Destructor
    /// Note the controller singleton is destroyed. This is essential to ensure
    /// a clean start between tests.
    virtual ~DControllerTest() {
        getController().reset();
    }

    /// @brief Convenience method that destructs and then recreates the
    /// controller singleton under test.  This is handy for tests within
    /// tests.
    void resetController() {
        getController().reset();
        getController();
    }

    /// @brief Static method which returns the instance of the controller
    /// under test.
    /// @return returns a reference to the controller instance.
    static DControllerBasePtr& getController() {
        return ((*instanceGetter_)());
    }

    /// @brief Returns true if the Controller's name matches the given value.
    ///
    /// @param should_be is the value to compare against.
    ///
    /// @return returns true if the values are equal.
    bool checkName(const std::string& should_be) {
        return (getController()->getName().compare(should_be) == 0);
    }

    /// @brief Returns true if the Controller's spec file name matches the
    /// given value.
    ///
    /// @param should_be is the value to compare against.
    ///
    /// @return returns true if the values are equal.
    bool checkSpecFileName(const std::string& should_be) {
        return (getController()->getSpecFileName().compare(should_be) == 0);
    }

    /// @brief Tests the existence of the Controller's application process.
    ///
    /// @return returns true if the process instance exists.
    bool checkProcess() {
        return (getController()->process_);
    }

    /// @brief Tests the existence of the Controller's IOService.
    ///
    /// @return returns true if the IOService exists.
    bool checkIOService() {
        return (getController()->io_service_);
    }

    /// @brief Gets the Controller's IOService.
    ///
    /// @return returns a reference to the IOService
    IOServicePtr& getIOService() {
        return (getController()->io_service_);
    }

    /// @brief Compares stand alone flag with the given value.
    ///
    /// @param value
    ///
    /// @return returns true if the stand alone flag is equal to the given
    /// value.
    bool checkStandAlone(bool value) {
        return (getController()->isStandAlone() == value);
    }

    /// @brief Sets the controller's stand alone flag to the given value.
    ///
    /// @param value is the new value to assign.
    ///
    void setStandAlone(bool value) {
        getController()->setStandAlone(value);
    }

    /// @brief Compares verbose flag with the given value.
    ///
    /// @param value
    ///
    /// @return returns true if the verbose flag is equal to the given value.
    bool checkVerbose(bool value) {
        return (getController()->isVerbose() == value);
    }

    /// @Wrapper to invoke the Controller's parseArgs method.  Please refer to
    /// DControllerBase::parseArgs for details.
    void parseArgs(int argc, char* argv[]) {
        getController()->parseArgs(argc, argv);
    }

    /// @Wrapper to invoke the Controller's init method.  Please refer to
    /// DControllerBase::init for details.
    void initProcess() {
        getController()->initProcess();
    }

    /// @Wrapper to invoke the Controller's establishSession method.  Please
    /// refer to DControllerBase::establishSession for details.
    void establishSession() {
        getController()->establishSession();
    }

    /// @Wrapper to invoke the Controller's launch method.  Please refer to
    /// DControllerBase::launch for details.
    void launch(int argc, char* argv[]) {
        optind = 1;
        getController()->launch(argc, argv);
    }

    /// @Wrapper to invoke the Controller's disconnectSession method.  Please
    /// refer to DControllerBase::disconnectSession for details.
    void disconnectSession() {
        getController()->disconnectSession();
    }

    /// @Wrapper to invoke the Controller's updateConfig method.  Please
    /// refer to DControllerBase::updateConfig for details.
    isc::data::ConstElementPtr updateConfig(isc::data::ConstElementPtr
                                            new_config) {
        return (getController()->updateConfig(new_config));
    }

    /// @Wrapper to invoke the Controller's executeCommand method.  Please
    /// refer to DControllerBase::executeCommand for details.
    isc::data::ConstElementPtr executeCommand(const std::string& command,
                                       isc::data::ConstElementPtr args){
        return (getController()->executeCommand(command, args));
    }

    /// @brief Callback that will generate shutdown command via the
    /// command callback function.
    static void genShutdownCallback() {
        isc::data::ElementPtr arg_set;
        DControllerBase::commandHandler(SHUT_DOWN_COMMAND, arg_set);
    }

    /// @brief Callback that throws an exception.
    static void genFatalErrorCallback() {
        isc_throw (DProcessBaseError, "simulated fatal error");
    }
};

}; // namespace isc::d2
}; // namespace isc

#endif

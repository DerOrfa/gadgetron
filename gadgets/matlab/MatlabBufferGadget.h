#pragma once

#include "gadgetron_matlab_export.h"
#include "Gadget.h"
#include "gadgetron_paths.h"
#include "hoNDArray.h"
#include "ismrmrd/ismrmrd.h"
#include "engine.h"     // Matlab Engine header

#include "ace/Synch.h"  // For the MatlabCommandServer
#include "ace/SOCK_Connector.h"
#include "ace/INET_Addr.h"


#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <boost/lexical_cast.hpp>
#include "mri_core_data.h"
// TODO:
//Make the port option work so that we can have multiple matlabs running, each with its own command server.
//Create a debug option to use evalstring and get back the matlab output on every function call.
//Finish the image stuff
//Is there a better way to kill the command server?
//Test on windows


namespace Gadgetron{

class MatlabBufferGadget:
    public Gadget1<IsmrmrdReconData >
{
public:
    MatlabBufferGadget(): Gadget1<IsmrmrdReconData >()
    {
        // Open the Matlab Engine on the current host
        GDEBUG("Starting MATLAB engine\n");
        if (!(engine_ = engOpen("matlab -nodesktop -nosplash"))) {
            // TODO: error checking!
            GDEBUG("Can't start MATLAB engine\n");
        } else {
            // Add ISMRMRD Java bindings jar to Matlab's path
            // TODO: this should be in user's Matlab path NOT HERE

            // Prepare a buffer for collecting Matlab's output
            char matlab_buffer_[2049] = "\0";
            engOutputBuffer(engine_, matlab_buffer_, 2048);

	    // Add the necessary paths to the matlab environment
	    // Java matlab command server
	    std::string gadgetron_matlab_path = get_gadgetron_home() + "/share/gadgetron/matlab";
	    std::string add_path_cmd = std::string("addpath('") + gadgetron_matlab_path + std::string("');");
            // Gadgetron matlab scripts
	    engEvalString(engine_, add_path_cmd.c_str());

		add_path_cmd = std::string("addpath('") + get_gadgetron_home() + "/share/ismrmrd/matlab');";
            // ISMRMRD matlab library
		engEvalString(engine_, add_path_cmd.c_str());

	    GDEBUG("%s", matlab_buffer_);
        }
    }

    ~MatlabBufferGadget()
    {
        // Close the Matlab engine
        GDEBUG("Closing down Matlab\n");
        engClose(engine_);
    }

    virtual int process(GadgetContainerMessage<IsmrmrdReconData> *);

protected:
    GADGET_PROPERTY(debug_mode, bool, "Debug mode", false);
    GADGET_PROPERTY(matlab_path, std::string, "Path to Matlab code", "");
    GADGET_PROPERTY(matlab_classname, std::string, "Name of Matlab gadget class", "");

    int process_config(ACE_Message_Block* mb)
    {
        std::string cmd;

        debug_mode_  = debug_mode.value();
        path_        = matlab_path.value();
        classname_   = matlab_classname.value();
        if (classname_.empty()) {
            GERROR("Missing Matlab Gadget classname in config!");
            return GADGET_FAIL;
        }

        GDEBUG("MATLAB Class Name : %s\n", classname_.c_str());

        //char matlab_buffer_[2049] = "\0";
        char matlab_buffer_[20481] = "\0";
        engOutputBuffer(engine_, matlab_buffer_, 20480);

   	// Instantiate the Java Command server
        // TODO: we HAVE to pause in Matlab to allow the java command server thread to start
                // add user specified path for this gadget
        if (!path_.empty()) {
            cmd = "addpath('" + path_ + "');";
            send_matlab_command(cmd);
        }

        // Put the XML Header into the matlab workspace
        std::string xmlConfig = std::string(mb->rd_ptr());
        mxArray *xmlstring = mxCreateString(xmlConfig.c_str());
        engPutVariable(engine_, "xmlstring", xmlstring);

        // Instantiate the Matlab gadget object from the user specified class
        // Call matlab gadget's init method with the XML Header
        // and the user defined config method
        cmd = "matgadget = " + classname_ + "();";
        cmd += "matgadget.init(xmlstring); matgadget.config();";
        if (send_matlab_command(cmd) != GADGET_OK) {
            GDEBUG("Failed to send matlab command.\n");
            return GADGET_FAIL;
        }

	mxDestroyArray(xmlstring);

        return GADGET_OK;
    }

    int send_matlab_command(std::string& command)
    {

            char matlab_buffer_[8193] = "\0";
            engOutputBuffer(engine_, matlab_buffer_, 8192);
            engEvalString(engine_, command.c_str());
            GDEBUG("%s\n", matlab_buffer_);
            return GADGET_OK;

    }


    std::string path_;
    std::string classname_;
    bool debug_mode_;

    Engine *engine_;
};

GADGET_FACTORY_DECLARE(MatlabBufferGadget);
}

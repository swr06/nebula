import os, platform, sys
import IDLC.idldocument as IDLDocument
import IDLC.idlattribute as IDLAttribute
import IDLC.idlcomponent as IDLComponent
import IDLC.idlprotocol as IDLProtocol
import sjson
import IDLC.filewriter
import genutil as util
import ntpath

class IDLCodeGenerator:
    def __init__(self):
        self.document = None
        self.documentPath = ""
        self.version = 0

    #------------------------------------------------------------------------------
    ##
    #
    def SetVersion(self, v):
        self.version = v

    #------------------------------------------------------------------------------
    ##
    #
    def SetDocument(self, input) :
        self.documentPath = input
        self.documentBaseName = os.path.splitext(input)[0]
        self.documentDirName = os.path.dirname(self.documentBaseName)

        head, tail = ntpath.split(self.documentBaseName)
        self.documentFileName = tail or ntpath.basename(head)

        fstream = open(self.documentPath, 'r')
        self.document = sjson.loads(fstream.read())
        fstream.close()


    #------------------------------------------------------------------------------
    ##
    #
    def GenerateHeader(self, hdrPath) :
        f = filewriter.FileWriter()
        f.Open(hdrPath)

        f.WriteLine("// NIDL #version:{}#".format(self.version))

        attributeLibraries = []

        # Add additional dependencies to document.
        if "dependencies" in self.document:
            for dependency in self.document["dependencies"]:
                fileName = '{}.h'.format(os.path.splitext(dependency)[0]).lower()
                attributeLibraries.append(fileName)

        if "messages" in self.document:
            attributeLibraries.append("game/messaging/message.h")

        attributeLibraries.append("core/sysfunc.h")
        attributeLibraries.append("util/stringatom.h")
        attributeLibraries.append("memdb/typeregistry.h")


        IDLDocument.WriteIncludeHeader(f)
        IDLDocument.WriteIncludes(f, self.document)
        IDLComponent.WriteIncludes(f, attributeLibraries)

        # Generate attributes include file
        if "properties" in self.document:            
            IDLDocument.WriteAttributeLibraryDeclaration(f)

        if "enums" in self.document:
                IDLDocument.BeginNamespace(f, self.document)
                IDLAttribute.WriteEnumeratedTypes(f, self.document)
                IDLDocument.EndNamespace(f, self.document)
                f.WriteLine("")

        if "properties" in self.document:
            IDLDocument.BeginNamespaceOverride(f, self.document, "Game")
            IDLAttribute.WriteAttributeHeaderDeclarations(f, self.document)
            IDLDocument.BeginNamespaceOverride(f, self.document, "Details")
            IDLAttribute.WriteAttributeHeaderDetails(f, self.document)
            IDLDocument.EndNamespaceOverride(f, self.document, "Details")
            IDLDocument.EndNamespaceOverride(f, self.document, "Game")
            f.WriteLine("")



        # Add additional dependencies to document.
        if "dependencies" in self.document:
            for dependency in self.document["dependencies"]:
                fstream = open(dependency, 'r')
                depDocument = sjson.loads(fstream.read())
                deps = depDocument["attributes"]
                # Add all attributes to this document
                self.document["attributes"].update(deps)
                fstream.close()


        # Generate components base classes headers
        hasMessages = "messages" in self.document
        hasComponents = "components" in self.document
        if hasComponents or hasMessages:
            IDLDocument.BeginNamespace(f, self.document)

            if hasMessages:
                IDLProtocol.WriteMessageDeclarations(f, self.document)

            if hasComponents:
                namespace = IDLDocument.GetNamespace(self.document)
                for componentName, component in self.document["components"].items():
                    componentWriter = IDLComponent.ComponentClassWriter(f, self.document, component, componentName, namespace)
                    componentWriter.WriteClassDeclaration()

            IDLDocument.EndNamespace(f, self.document)

        f.Close()
        return


    #------------------------------------------------------------------------------
    ##
    #
    def GenerateSource(self, srcPath, hdrPath) :
        f = filewriter.FileWriter()
        f.Open(srcPath)        
        f.WriteLine("// NIDL #version:{}#".format(self.version))              
        head, tail = ntpath.split(hdrPath)
        hdrInclude = tail or ntpath.basename(head)

        head, tail = ntpath.split(srcPath)
        srcFileName = tail or ntpath.basename(head)

        IDLDocument.WriteSourceHeader(f, srcFileName)        
        IDLDocument.AddInclude(f, hdrInclude)
        
        hasMessages = "messages" in self.document

        if hasMessages:            
            IDLDocument.AddInclude(f, "scripting/bindings.h")

        if "properties" in self.document:
            IDLDocument.BeginNamespaceOverride(f, self.document, "Game")
            IDLDocument.BeginNamespaceOverride(f, self.document, "Details")
            IDLAttribute.WriteAttributeSourceDefinitions(f, self.document)
            IDLDocument.EndNamespaceOverride(f, self.document, "Details")
            IDLDocument.EndNamespaceOverride(f, self.document, "Game")
            f.WriteLine("")

        # Add additional dependencies to document.
        if "dependencies" in self.document:
            for dependency in self.document["dependencies"]:
                fstream = open(dependency, 'r')
                depDocument = sjson.loads(fstream.read())
                deps = depDocument["attributes"]
                # Add all attributes to this document
                self.document["attributes"].update(deps)
                fstream.close()

        hasComponents = "components" in self.document
        if hasComponents or hasMessages:
            IDLDocument.BeginNamespace(f, self.document)

            if hasMessages:
                IDLProtocol.WriteMessageImplementation(f, self.document)

            if hasComponents:
                namespace = IDLDocument.GetNamespace(self.document)
                for componentName, component in self.document["components"].items():
                    f.WriteLine("")
                    componentWriter = IDLComponent.ComponentClassWriter(f, self.document, component, componentName, namespace)
                    componentWriter.WriteClassImplementation()
                    f.WriteLine("")

            IDLDocument.EndNamespace(f, self.document)

        f.Close()

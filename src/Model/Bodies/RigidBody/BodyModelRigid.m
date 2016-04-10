% System kinematics and dynamics of a rigid body
%
% Author        : Darwin LAU
% Created       : 2011
% Description    :
%   This class inherits from BodyModel and assumes that the body is a rigid
%   body. Currently this is the only implementation of the BodyModel as no
%   elastic/deformable bodies are considered.
classdef BodyModelRigid < BodyModel

    methods
        function bk = BodyModelRigid(id, name, jointType)
            bk@BodyModel(id, name, jointType);
        end

        function update(obj, q, q_dot, q_ddot)
            % Can update states (such as local attachments) first if necessary
            % Since bodies and attachment locations do not change, no need to update anything here

            % Update standard things from kinematics
            update@BodyModel(obj, q, q_dot, q_ddot);

            % Post updates if necessary
        end
    end

    methods (Static)
        function [bk] = LoadXmlObj(xmlobj)
            % <link_rigid> tag
            id = str2double(char(xmlobj.getAttribute('num')));
            name = char(xmlobj.getAttribute('name'));
            
            % <joint_type> tag
            jointTypeObj = xmlobj.getElementsByTagName('joint_type').item(0);
            joint_type = char(jointTypeObj.getFirstChild.getData);
            
            % Generate the rigid body object
            bk = BodyModelRigid(id, name, JointType.(joint_type));
            
            % <physical> tag
            physicalObj = xmlobj.getElementsByTagName('physical').item(0);
            % <mass>
            bk.m = str2double(physicalObj.getElementsByTagName('mass').item(0).getFirstChild.getData);
            % <com_location>
            bk.r_G = XmlOperations.StringToVector3(char(physicalObj.getElementsByTagName('com_location').item(0).getFirstChild.getData));
            % <end_location>
            bk.r_Pe = XmlOperations.StringToVector3(char(physicalObj.getElementsByTagName('end_location').item(0).getFirstChild.getData));
            % <inertia>
            inertiaObj = physicalObj.getElementsByTagName('inertia').item(0);
            I_Gxx = str2double(inertiaObj.getElementsByTagName('Ixx').item(0).getFirstChild.getData);
            I_Gyy = str2double(inertiaObj.getElementsByTagName('Iyy').item(0).getFirstChild.getData);
            I_Gzz = str2double(inertiaObj.getElementsByTagName('Izz').item(0).getFirstChild.getData);
            I_Gxy = str2double(inertiaObj.getElementsByTagName('Ixy').item(0).getFirstChild.getData);
            I_Gxz = str2double(inertiaObj.getElementsByTagName('Ixz').item(0).getFirstChild.getData);
            I_Gyz = str2double(inertiaObj.getElementsByTagName('Iyz').item(0).getFirstChild.getData);
            bk.I_G = [I_Gxx I_Gxy I_Gxz; ...
                I_Gxy I_Gyy I_Gyz; ...
                I_Gxz I_Gyz I_Gzz];
            
            % <parent> tag
            parentObj = xmlobj.getElementsByTagName('parent').item(0);
            % <num>
            bk.parentLinkId = str2double(char(parentObj.getElementsByTagName('num').item(0).getFirstChild.getData));
            % <location>
            bk.r_Parent = XmlOperations.StringToVector3(char(parentObj.getElementsByTagName('location').item(0).getFirstChild.getData));
        end
    end

end
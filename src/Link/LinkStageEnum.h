// LinkStageEnum.h

#ifndef _LINK_STAGE_ENUM_h
#define _LINK_STAGE_ENUM_h

enum LinkStageEnum
{
	Disabled,
	Booting,
	AwaitingLink,
	//TransitionToLinking,
	Linking,
	//TransitionToLinked,
	Linked
};
#endif
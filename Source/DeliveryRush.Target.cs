using UnrealBuildTool;
using System.Collections.Generic;

public class DeliveryRushTarget : TargetRules
{
    public DeliveryRushTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("DeliveryRush");
    }
}

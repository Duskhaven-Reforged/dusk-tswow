import { ipaths } from "../util/Paths";
import { isWindows } from "../util/Platform";
import { wsys } from "../util/System";
import { bpaths, spaths } from "./CompilePaths";

export namespace ClientExtensions64 {
    export async function create(cmake: string) {
        if(isWindows())
        {
            // build locally
            wsys.exec(`${cmake} `
            + `-A x64`
            + ` -S "${spaths.misc.client_extensions_64.abs().get()}" `
            + ` -B "${bpaths.client_extensions_64.abs().get()}"`
            + ` -DBOOST_ROOT="${bpaths.boost.boost_1_82_0.abs().get()}"`
            , 'inherit');

            wsys.exec(`${cmake}`
                + ` --build "${bpaths.client_extensions_64.abs().get()}"`
                + ` --config Release`
                , 'inherit');
            bpaths.client_extensions_64.exe_path.copy(ipaths.bin.ClientExtensions64_exe)
            bpaths.client_extensions_64.DiscordPartnerSDK_dll_path.copy(ipaths.bin.DiscordPartnerSDK_dll)
        }
    }
}
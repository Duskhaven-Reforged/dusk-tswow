import * as chokidar from "chokidar";
import * as fs from "fs";
import { WFile } from "../util/FileTree";
import { ipaths } from "../util/Paths";
import { term } from "../util/Terminal";

export class Crashes {
    static latestCallStack(crashDir: string) {
        try {
            if(!fs.existsSync(crashDir)) {
                return '';
            }

            const latest = fs.readdirSync(crashDir)
                .filter(x => x.endsWith('.txt'))
                .map(name => {
                    const full = `${crashDir}\\${name}`;
                    return { full, mtime: fs.statSync(full).mtimeMs };
                })
                .sort((a,b) => b.mtime - a.mtime)[0];

            if(!latest) {
                return '';
            }

            const content = fs.readFileSync(latest.full, 'utf8');
            const callStackIndex = content.indexOf('Call stack:');
            if(callStackIndex < 0) {
                return '';
            }

            const afterHeader = content.slice(callStackIndex).split(/\r?\n/);
            const collected: string[] = [];
            let seenHeader = false;
            for(const line of afterHeader) {
                if(!seenHeader) {
                    if(line.startsWith('Call stack:')) {
                        seenHeader = true;
                        collected.push(line);
                    }
                    continue;
                }

                if(line.startsWith('Call stack:')) {
                    break;
                }

                if(collected.length > 0 && line.trim().length === 0) {
                    break;
                }

                collected.push(line);
            }

            return collected.join('\n').trim();
        } catch {
            return '';
        }
    }

    static initialize() {
        if (process.argv.includes('nowatch') || process.argv.includes('nowatch-strict'))
        {
            return
        }

        term.debug('misc', `Initializing crashlog handler`)
        chokidar.watch(ipaths.modules.abs().get(),{
            ignored: [
                /build$/
              , /Buildings$/
              , /maps$/
              , /vmaps$/
              , /mmaps$/
              , /dbc$/
              , /dbc_source/
              , /datascripts$/
              , /assets$/
              , /addon$/
              , /livescripts$/
              , /addons$/
              , /shared$/
              , /luaxml$/
              , /luaxml_source/
              , /(^|[\/\\])\../
          ]
        }).on('add',sfile=>{
            let file = new WFile(sfile);
            if(file.basename(1).get() !== 'Crashes') return;
            const ctime = file.ctime();
            const [_,type] = file.basename().split('_');
            const realmPath = file.dirname().dirname()
                .relativeTo(ipaths.modules)
                .split('\\').join('/').split('/').join('.')
                .split('realms.').join('')
                .split('datasets.').join('')

            ipaths.Crashes.mkdir();

            // need to wait for tc to actually write the file
            setTimeout(()=>{
                let i = 0;
                let int = setInterval(()=>{
                    i++;
                    if(i>=10) {
                        clearInterval(int);
                    }
                    try {
                        fs.copyFileSync(file.abs().get(),ipaths.Crashes.join(
                            `${ctime.getFullYear()}-${ctime.getMonth()+1}-${ctime.getDate()+1}.`
                            + `${ctime.getHours()}-${ctime.getMinutes()}-${ctime.getSeconds()}.`
                            + `${type}-${realmPath}.${file.extension()}`).get())
                        file.remove()
                        clearInterval(int)
                    } catch(err) {}
                },500)
            },2000)


        })
    }
}

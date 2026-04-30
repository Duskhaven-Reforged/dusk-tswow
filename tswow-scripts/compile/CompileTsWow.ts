/*
 * This file is part of tswow (https://github.com/tswow)
 *
 * Copyright (C) 2020 tswow <https://github.com/tswow/>
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
import { Args } from '../util/Args';
import { commands } from '../util/Commands';
import { ipaths } from '../util/Paths';
import { isWindows } from '../util/Platform';
import { term } from '../util/Terminal';
import { setContext } from '../util/TSWoWContext';
import { SevenZipInstall } from './7Zip';
import { ADTCreator } from './ADTCreator';
import { BLPConverter } from './BLPConverter';
import { Boost } from './Boost';
import { isInteractive } from './BuildConfig';
import { ClientExtensions } from './ClientExtensions';
import { ClientExtensions64 } from './ClientExtensions64';
import { CMake } from './Cmake';
import { bpaths, spaths } from './CompilePaths';
import { Config } from './Config';
import { IMInstall } from './ImageMagick';
import { MPQBuilder } from './MPQBuilder';
import { MySQL } from './MySQL';
import { NodeJS } from './Node';
import { OpenSSL } from './OpenSSL';
import { Scripts } from './Scripts';
import { TrinityCore } from './TrinityCore';
setContext('build');

let buildingScripts = false;

const installedPrograms =
    [
          'trinitycore'
        , 'trinitycore-release'
        , 'trinitycore-relwithdebinfo'
        , 'trinitycore-debug'
        , 'mpqbuilder'
        , 'blpconverter'
        , 'config'
        , 'database'
        , 'full'
        , 'scripts'
        , 'clean-install'
        , 'clean-build'
        , 'release'
        , 'adtcreator'
        , 'client-extensions'
        , 'client-extensions-64'
    ];

async function compile(type: string, compileArgs: string[]) {
    // Load necessary libraries
    const types = type.split(' ');
    function isType(check: string) {
        return types.includes('full') || types.includes('release') || types.includes(check);
    }

    const cmake = isWindows() ? (await CMake.find()).get() : 'cmake';
    term.log('build',`Found cmake at ${cmake}`);
    const openssl = isWindows() ? (await OpenSSL.find()).get() : 'openssl';
    term.log('build',`Found OpenSSL at ${openssl}`);
    const mysql = isWindows() ? await MySQL.find() : 'mysql';
    term.log('build',`Found MySQL at ${mysql}`);
    const boost = isWindows() ? await Boost.install() : 'boost';
    term.log('build',`Found Boost at ${boost}`);
    await NodeJS.install();
    if (isWindows()) { await SevenZipInstall.install(); }
    if (isWindows()) { await IMInstall.install() }

    if (types.includes('full') || types.includes('release')) {
        await TrinityCore.install(cmake, openssl, mysql, 'RelWithDebInfo', compileArgs.concat(['dynamic']));
    } else {
        if (type == 'trinitycore-release') { await TrinityCore.install(cmake, openssl, mysql, 'Release', compileArgs); }
        if (isType('trinitycore') || isType('trinitycore-relwithdebinfo')) { await TrinityCore.install(cmake, openssl, mysql, 'RelWithDebInfo', compileArgs); }
        if (type == 'trinitycore-debug') { await TrinityCore.install(cmake, openssl, mysql, 'Debug', compileArgs); }
    }

    if (isType('mpqbuilder')) { await MPQBuilder.create(cmake); }
    if (isType('blpconverter')) { await BLPConverter.install(cmake); }
    if (isWindows() && isType('adtcreator')) { await ADTCreator.create(cmake); }
    if (isType('client-extensions')) { await ClientExtensions.create(cmake); }
    if (isType('client-extensions-64')) { await ClientExtensions64.create(cmake); }

    if (!buildingScripts && isType('scripts')) {
        await Scripts.build();
        buildingScripts = true;
    }

    if(isType('config')) {
        await Config.create();
    }

    if (types.includes('release')) {
        term.log('build',`Creating ${bpaths.release_7z.get()}`);
        SevenZipInstall.makeArchive(bpaths.release_7z.abs().get(), ipaths.abs().get());
    }

    term.log('build','Installation successful!');
}

async function main() {
    term.Initialize(
          bpaths.terminal_history.get()
        , 100
        , process.argv.includes('--displayTimestamps')
        , process.argv.includes('--displayNames')
        );
    const build = commands.addCommand('build');
    await compile('scripts', []);

    for (const val of installedPrograms) {
        build.addCommand(val, '', `Builds ${val}`, async(args) => await compile(val, args));
    }

    build.addCommand('base', '', 'Builds only base dependencies', async(args) => await compile('', args));

    commands.addCommand('headers','','',async(args)=>{
        TrinityCore.headers(Args.hasFlag('global-only',args));
    });

    commands.enterLoop();
}

function normalizeBuildTarget(target: string) {
    switch (target) {
        case 'client-extension':
            return 'client-extensions';
        case 'client-extension-64':
            return 'client-extensions-64';
        default:
            return target;
    }
}

function getBuildTargetFromArgs() {
    const rawTarget = process.argv
        .slice(2)
        .find(x => {
            if (x.startsWith('-')) {
                return false;
            }

            const target = normalizeBuildTarget(x);
            return target === 'base' || installedPrograms.includes(target);
        });

    return rawTarget === undefined
        ? process.argv.includes('--release') ? 'release' : 'full'
        : normalizeBuildTarget(rawTarget);
}

(async function(){
    if(!spaths.tswow_scripts.wotlk.global_d_ts.exists()) {
        TrinityCore.headers(true);
    }

    if(Args.hasFlag('gdts-only', [process.argv])) {
        TrinityCore.headers(true);
        process.exit(0);
    }

    if(isInteractive) {
        main();
    } else {
        await compile(getBuildTargetFromArgs(),[]);
        process.exit(0);
    }
}())

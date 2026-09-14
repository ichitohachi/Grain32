(function () {
    var wanted = ["ADBE Skeleton", "EMOTO Grain32"];
    var lines = ["After Effects " + app.version];
    for (var j = 0; j < wanted.length; j++) {
        var found = false;
        for (var i = 0; i < app.effects.length; i++) {
            if (app.effects[i].matchName === wanted[j]) {
                lines.push("FOUND: " + wanted[j] + " / " + app.effects[i].displayName);
                found = true;
                break;
            }
        }
        if (!found) lines.push("MISSING: " + wanted[j]);
    }
    alert(lines.join("\n"));
}());

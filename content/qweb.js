/**
 * Make a request to a qweb POST callback
 * 
 * @param {string} path qweb registered path for callback
 * @param {Uint8Array} data raw byte data
 * @param {CallableFunction(string)} success Callback for a successful response
 * @param {CallableFunction(string)} failure Callback for a failure response
 */
function qweb(path, data, success, failure){
    const http = new XMLHttpRequest();
    http.open("POST", path);
    http.setRequestHeader('Content-type', 'application/octet-stream');
    http.onloadend = () => {
        if (http.status == 200) {
            if (success) success(http.responseText);
        } else {
            if (failure) failure(http.responseText);
        }
    }
    http.send(data);
}


/**
 * Convert string to bytes
 * @param {string} s string
 * @returns Nullterminated byte array based on string
 */
function qweb_s2b(s) { return Uint8Array.from((s+'\0').split("").map(x => x.charCodeAt())) }


/**
 * Convert json to bytes
 * @param {any} json any stringifiable json
 * @returns Null terminated byte array based on stringified json
 */
function qweb_j2b(json) { return qweb_s2b(JSON.stringify(json)) }
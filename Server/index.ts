import { serve } from "bun"; 

console.log("Server is running on http://localhost:3000");
console.log("Restarted at:", Date.now());

const website = Bun.file("./index.html");

serve({
    port: 3000,
    fetch(req) {
        return new Response(website);
    },
});